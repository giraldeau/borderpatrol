
type eid_t = int
type pid_t = int
type tid_t = Int64.t
type fd_t  = int
type rdv_t = string
type rid_t = int
type msgid_t = int

type event_type =
    | Start of pid_t * tid_t
    (* system calls *)
    | Fork of pid_t * tid_t
    | Exec of tid_t
    | PollBegin
    | Poll of bool * fd_t
    | Socket of fd_t
    | Pipe of fd_t * fd_t
    | Dup of fd_t * fd_t
    | Close of fd_t
    | Exit
    | Abort
    | Shutdown
    | Accept of fd_t * rdv_t
    | Connect of fd_t * rdv_t
    | Open of fd_t
    | WriteBegin of fd_t
    | Write of fd_t
    | ReadBegin of fd_t
    | Read of fd_t
    (* messages *)
    | ProtocolInit of fd_t
    | MessageSend of string * fd_t * msgid_t
    | MessageRecv of string * fd_t * msgid_t
    (* the rest *)
    | SkipEvent
    | TodoEvent of string
    | Unknown

type event = {
    e_eid: eid_t;
    e_pid: pid_t;
    e_tid: tid_t;
    e_ptid: tid_t;
    e_ts: Int64.t;
    e_sc_str: string;
    e_rv: int;
    e_prog: string;
    e_args: int list;
    e_str: string;
}

type stream = { s_type: event_type; s_ev: event; }

exception Strange_error


(* ********************************************************************** *)
(* Context Data Types *)

type mid_t = { m_tid: tid_t; m_pid: pid_t }

type dgn_t = { d_mid: mid_t; d_rid: rid_t; d_new: bool }

type conn_id_t = GenSym of int | SunPath of string | HostPort of rdv_t

type conn_t = { c_id: conn_id_t; }

type msg_t = { msg_pid: pid_t; msg_ev: eid_t; msg_send: bool; msg_prot: string; msg_conn: string; msg_id: msgid_t; msg_rid: rid_t }

type ctx_t = {
    c_dgn:  dgn_t list;
    c_fd:   ((pid_t * fd_t), conn_t) Hashtbl.t;
    c_conn: ((conn_t * msgid_t), rid_t) Hashtbl.t;
    c_msgs: msg_t list;
  }

let string_of_mid m =
  Printf.sprintf "%d_%Ld" m.m_pid m.m_tid
;;

let string_of_conn c =
  String.concat "-" [
      (match c.c_id with
	| GenSym(i) -> (string_of_int i)
	| SunPath(s) -> Printf.sprintf "sun=%s" s
	| HostPort(rdv) -> Printf.sprintf "sin=%s" rdv);
    ]
;;

let conn_eq c d =
  match (c.c_id,d.c_id) with
    | (GenSym(a),GenSym(b)) -> a = b
    | (SunPath(s),SunPath(t)) -> s = t
    | (HostPort(rdv1),HostPort(rdv2)) -> rdv1 = rdv2
    | _ -> false
;;


(* ********************************************************************** *)

let mid_of_pt pid tid =
  { m_tid=tid; m_pid=pid }
;;

let mideq m1 m2 =
  m1.m_pid = m2.m_pid && m1.m_tid = m2.m_tid
;;

let matchportbyname name port =
  (Str.string_match 
     (Str.regexp ("^.*:" ^ (string_of_int
			      (Swig.get_int
				 (Bridge._getportbyname
				    (Swig.C_string name)))) ^
		    "$"))
     port 0)
;;
