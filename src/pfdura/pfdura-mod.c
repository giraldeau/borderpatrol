/* pfdura-mod.c
 *
 * kernel module that installs kprobes to track the duration
 * of page faults per process
 *
 * PID_MAX_DEFAULT is used; if /proc/sys/kernel/pid_max is changed
 * then the bit vector used to track PIDs will break
 */


#include <linux/kprobes.h>
#include <linux/mm.h>
#include <linux/module.h>
#include <linux/string.h>
#include <linux/threads.h>
#include <linux/time.h>
#include <linux/relay.h>
#include <linux/debugfs.h>
#include <asm/bitops.h>
#include "pfdura.h"
#include "config.h"

#define APP_DIR			"pfdura"
#define PROBE_FUNC	handle_mm_fault
/* #define ENTRY_ADDR	0xc01974e0 */
/* #define EXIT_ADDR		0xc018da80 */
#define SUBBUF_SIZE	sizeof (struct pfdura_t)*1024
#define N_SUBBUFS		4


static struct rchan 		*chan;
static struct dentry		*dir;
static struct dentry		*pid_on, *pid_off;
static struct file_operations	pid_on_fops, pid_off_fops;
static unsigned long		track_pids[(PID_MAX_DEFAULT/8)/sizeof (long)];

/*
 * create_buf_file() callback.  Creates relay file in debugfs.
 */
static struct dentry *create_buf_file_handler(const char *filename,
                                              struct dentry *parent,
                                              int mode,
                                              struct rchan_buf *buf,
                                              int *is_global)
{
        return debugfs_create_file(filename, 0644, parent, buf,
                                   &relay_file_operations);
}

/*
 * remove_buf_file() callback.  Removes relay file from debugfs.
 */
static int remove_buf_file_handler(struct dentry *dentry)
{
        debugfs_remove(dentry);

        return 0;
}

/*
 * relay interface callbacks
 */
static struct rchan_callbacks pfdura_relay_callbacks =
{
        .create_buf_file = create_buf_file_handler,
        .remove_buf_file = remove_buf_file_handler,
};

static
size_t
get_pid_from_buf(const char __user *buffer, size_t count, long *pid)
{
	char buf[16];
	char *tmp;

	if (count > sizeof (buf))
		return -EINVAL;

	memset(buf, 0, sizeof (buf));

	if (copy_from_user(buf, buffer, count))
		return -EFAULT;

	*pid = simple_strtol(buf, &tmp, 10);
	if (tmp == buf)
		return -EINVAL;

	return count;
}


static
ssize_t
pid_on_write(struct file *filp, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	long r, pid;

	r = get_pid_from_buf(buffer, count, &pid);
	if (r != count)
		return r;
	if (pid >= PID_MAX_DEFAULT) {
		printk("pfdura-mod: invalid PID\n");
		return -EINVAL;
	}
	set_bit(pid, track_pids);
	printk("pfdura-mod: tracking %li\n", pid);

	return count;
}


static
struct file_operations
pid_on_fops = {
	.owner =	THIS_MODULE,
	.write =	pid_on_write,
};


static
ssize_t
pid_off_write(struct file *filp, const char __user *buffer,
		size_t count, loff_t *ppos)
{
	long r, pid;

	r = get_pid_from_buf(buffer, count, &pid);
	if (r != count)
		return r;
	if (pid >= PID_MAX_DEFAULT) {
		printk("pfdura-mod: invalid PID\n");
		return -EINVAL;
	}

	/* PID 0 turns off all PID tracking. The memset is not atomic,
	 * but it shouldn't matter if we get a few extra reports.
	 */
	if (pid == 0) {
		memset(track_pids, 0, sizeof (track_pids));
		printk("pfdura-mod: stopped tracking all PIDs\n");
		return count;
	}

	if (!test_bit(pid, track_pids)) {
		printk("pfdura-mod: error: not tracking PID %li\n", pid);
		return -EINVAL;
	}
	clear_bit(pid, track_pids);
	printk("pfdura-mod: stopped tracking %li\n", pid);

	return count;
}


static
struct file_operations
pid_off_fops = {
	.owner =	THIS_MODULE,
	.write =	pid_off_write,
};


static
int
entry_probe(struct kprobe *p, struct pt_regs *regs)
{
	struct pfdura_t	pfd;
	static int suspended = 0;

	if (current->tgid >= PID_MAX_DEFAULT) {
		if (!suspended) {
			printk("pfdura-mod: current PID is out of range!\n");
			suspended = 1;
		}
		return 0;
	}
	suspended = 0;

	if (!test_bit(current->tgid, track_pids))
		return 0;

	pfd.tgid = current->tgid;
	pfd.pid = current->pid;
	pfd.complete = 0;
	do_gettimeofday(&pfd.tv);
	relay_write(chan, &pfd, sizeof (pfd));

	return 0;
}


static
struct kprobe
kp_entry = {
	.pre_handler = entry_probe,
	.post_handler = NULL,
	.fault_handler = NULL,
	.addr = NULL,
};


static
int
exit_probe(struct kprobe *p, struct pt_regs *regs)
{
	struct pfdura_t	pfd;
	static int suspended = 0;

	if (current->tgid >= PID_MAX_DEFAULT) {
		if (!suspended) {
			printk("pfdura-mod: current PID is out of range!\n");
			suspended = 1;
		}
		return 0;
	}
	suspended = 0;

	if (!test_bit(current->tgid, track_pids))
		return 0;

	pfd.tgid = current->tgid;
	pfd.pid = current->pid;
	pfd.complete = 1;
	do_gettimeofday(&pfd.tv);
	relay_write(chan, &pfd, sizeof (pfd));

	return 0;
}


static
struct kprobe
kp_exit = {
	.pre_handler = exit_probe,
	.post_handler = NULL,
	.fault_handler = NULL,
	.addr = NULL,
};


static
void
cleanup_all(void)
{
	if (pid_on) {
		debugfs_remove(pid_on);
		pid_on = NULL;
	}
	if (pid_off) {
		debugfs_remove(pid_off);
		pid_off = NULL;
	}
	if (dir) {
		debugfs_remove(dir);
		dir = NULL;
	}
}


int
init_module(void)
{
	memset(track_pids, 0, sizeof (track_pids));

	dir = debugfs_create_dir(APP_DIR, NULL);
	if (!dir) {
		printk("pfdura-mod: debugfs_create_dir() failed\n");
		goto fail;
	}

	pid_on = debugfs_create_file("on", 0666, dir, NULL, &pid_on_fops);
	if (!pid_on) {
		printk("pfdura-mod: debugfs_create_file() failed\n");
		goto fail;
	}

	pid_off = debugfs_create_file("off", 0666, dir, NULL, &pid_off_fops);
	if (!pid_off) {
		printk("pfdura-mod: debugfs_create_file() failed\n");
		goto fail;
	}

	chan = relay_open("cpu", dir, SUBBUF_SIZE, N_SUBBUFS,
                    &pfdura_relay_callbacks, NULL);
	if (!chan) {
		printk("pfdura-mod: relay_open() failed\n");
		goto fail;
	}

  kp_entry.addr = (kprobe_opcode_t *) ENTRY_ADDR;
  kp_exit.addr = (kprobe_opcode_t *) EXIT_ADDR;

	register_kprobe(&kp_entry);
	register_kprobe(&kp_exit);
	printk("pfdura-mod: probes registered\n");

	return 0;

fail:
	cleanup_all();
	return -ENOMEM;
}


void
cleanup_module(void)
{
	unregister_kprobe(&kp_entry);
	unregister_kprobe(&kp_exit);
	memset(track_pids, 0, sizeof (track_pids));
	relay_close(chan);
	chan = NULL;
	cleanup_all();
	printk("pfdura-mod: probes unregistered\n");
}


MODULE_LICENSE("GPL");

