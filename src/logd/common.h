#ifndef COMMON_H
#define COMMON_H

#define BUF_DEBUG 0

logentry_thaw_t *get_le(int fd,int persist);
void free_le(logentry_thaw_t *le);

#endif
