
open List
open Events

open Postgresql

exception Unmatched_event of string
exception Parse_error of string

let conn = new connection ~conninfo:"dbname=debug" ()

(* **************************************************************** *)
(* Logfile Parsing                                                  *)
(* **************************************************************** *)

let mk_eventtype ev =
  if ev.e_rv < 0 then SkipEvent else
    if (Str.string_match (Str.regexp "_BUF_") ev.e_sc_str 0) then SkipEvent else
  match ev.e_sc_str with
    | "SYS_RS" -> SkipEvent
    | "SYS_POLLMAX" -> SkipEvent
    | "SYS_POLL_CACHE" -> SkipEvent
    | "SYS_BUF_PRE" -> SkipEvent
    | "SYS_BUF_REFILLED" -> SkipEvent
    | "SYS_BUF_PPOFS" -> SkipEvent
    | "SYS_BUF_RETURNED" -> SkipEvent
    | "SYS_BUF_SHORT1" -> SkipEvent
    | "SYS_BUF_SHORT2" -> SkipEvent
    | "SYS_BUF_MOVED" -> SkipEvent
    | "SYS_PP_ISO_POLL" -> SkipEvent
    | "SYS_PP_PROP_L" -> SkipEvent
    | "SYS_PP_MSG_HOOK" -> SkipEvent
    | "SYS_REMEMBER" -> SkipEvent
    | "SYS_HTTPS_READ" -> SkipEvent
    | "SYS_SELECT_STATUS" -> SkipEvent
    | "SYS_PP_UNID1" -> SkipEvent
    | "SYS_PP_UNID2" -> SkipEvent
    | "SYS_PP_UNID3" -> SkipEvent
    | "SYS_PP_SUN" -> SkipEvent
    | "SYS_PP_FCGI_HMM" -> SkipEvent
  | "Syscall_render_error" -> SkipEvent
(*    | "" -> SkipEvent
    | "" -> SkipEvent
    | "" -> SkipEvent
    | "" -> SkipEvent *)
    | "SYS_RLOOK" -> SkipEvent
    | "SYS_RDONE" -> SkipEvent
    | "SYS_RSEND" -> SkipEvent
    | "SYS_PP_ISO_READ" -> SkipEvent


    | "SYS_START"         -> Start(ev.e_pid,ev.e_tid)
    | "SYS_FORK_BEGIN"    -> SkipEvent
    (*| "SYS_FORK"          -> if ev.e_rv = 0 then Fork((nth args 0),ptid) else SkipEvent*)
    | "SYS_FORK"          -> Fork((nth ev.e_args 0),ev.e_ptid)

    | "SYS_EXEC"          -> SkipEvent (* we dont care about these events, *)
    | "SYS_EXECL"         -> SkipEvent (*   because we will soon get an EXEC_CHILD *)
    | "SYS_EXECLP"        -> SkipEvent
    | "SYS_EXECVE"        -> SkipEvent
    | "SYS_EXECVP"        -> SkipEvent
    | "SYS_EXECLE"        -> SkipEvent
    | "SYS_EXECV"         -> SkipEvent
    | "SYS_EXECP"         -> SkipEvent

    | "SYS_EXEC_CHILD"    -> Exec(ev.e_ptid)

    | "SYS_UNSETENV"      -> SkipEvent
    | "SYS_SETENV"        -> SkipEvent
    | "SYS_GETENV"        -> SkipEvent

    | "SYS_DUP"           -> Dup((nth ev.e_args 0),ev.e_rv)
    | "SYS_DUP2"          -> Dup((nth ev.e_args 0),ev.e_rv)
    | "SYS_DUP_FCNTL"     -> Dup((nth ev.e_args 0),ev.e_rv)

    | "SYS_SOCKET"        -> Socket(ev.e_rv)
    | "SYS_BIND"          -> SkipEvent
    | "SYS_LISTEN"        -> SkipEvent
    | "SYS_PIPE"          -> Pipe( (nth ev.e_args 0), (nth ev.e_args 1) )

    | "SYS_OPEN"          -> Open(ev.e_rv)
    | "SYS_CLOSE"         -> Close(nth ev.e_args 0)
    | "SYS_ACCEPT_BEGIN"  -> SkipEvent
    | "SYS_ACCEPT"        -> Accept(ev.e_rv, ev.e_str )
    | "SYS_CONNECT_BEGIN" -> SkipEvent
    | "SYS_CONNECT"       -> Connect( (nth ev.e_args 0), ev.e_str )

    | "SYS_WRITE_BUFFER"  -> SkipEvent
    | "SYS_READ_BUFFER"   -> SkipEvent

    | "SYS_WRITE_BEGIN"   -> WriteBegin(nth ev.e_args 0)
    | "SYS_WRITE"         -> Write(nth ev.e_args 0)
    | "SYS_WRITEV_BEGIN"  -> WriteBegin(nth ev.e_args 0)
    | "SYS_WRITEV"        -> Write(nth ev.e_args 0)
    | "SYS_SEND_BEGIN"    -> WriteBegin(nth ev.e_args 0)
    | "SYS_SEND"          -> Write(nth ev.e_args 0)
    | "SYS_SENDTO_BEGIN"  -> WriteBegin(nth ev.e_args 0)
    | "SYS_SENDTO"        -> Write(nth ev.e_args 0)
    | "SYS_SENDFILE_BEGIN"-> WriteBegin(nth ev.e_args 0)
    | "SYS_SENDFILE"      -> Write(nth ev.e_args 0)
    | "SYS_SENDFILE64_BEGIN"-> WriteBegin(nth ev.e_args 0)
    | "SYS_SENDFILE64"      -> Write(nth ev.e_args 0)
    | "SYS_READ_BEGIN"    -> ReadBegin(nth ev.e_args 0)
    | "SYS_READ"          -> Read(nth ev.e_args 0)
    | "SYS_RECV_BEGIN"    -> ReadBegin(nth ev.e_args 0)
    | "SYS_RECV"          -> Read(nth ev.e_args 0)
    | "SYS_RECVFROM_BEGIN"-> ReadBegin(nth ev.e_args 0)
    | "SYS_RECVFROM"      -> Read(nth ev.e_args 0)
    | "SYS_PAGEFAULT_BEGIN" -> TodoEvent("pf: don't know pagefault's fd")
    | "SYS_PAGEFAULT"       -> TodoEvent("pf: don't know pagefault's fd")

    | "SYS_PTH_CREATE_BEGIN"  -> TodoEvent("pth")
    | "SYS_PTH_CREATE_CHILD"  -> TodoEvent("pth")
    | "SYS_PTH_CREATE_PARENT" -> TodoEvent("pth")
    | "SYS_PP_PROP"       -> SkipEvent

    | "SYS_PP_DATA"       -> SkipEvent
    | "SYS_PP_ACTIVE"       -> SkipEvent
    | "SYS_DATA_HERE"       -> SkipEvent
    | "SYS_PP_ID"         -> SkipEvent
    | "SYS_PP_INIT"       -> ProtocolInit(nth ev.e_args 1)
    | "SYS_PP_SHUTDOWN"   -> SkipEvent
    | "SYS_PP_MSG_SEND"   -> MessageSend((Syscalls.str_of_ppid (nth ev.e_args 0)), (* prot id *)
					 (nth ev.e_args 1),  (* fd *)
					 (nth ev.e_args 2))  (* msgid *)
    | "SYS_PP_MSG_RECV"   -> MessageRecv((Syscalls.str_of_ppid (nth ev.e_args 0)), (* prot id *)
					(nth ev.e_args 1),  (* fd *)
					 (nth ev.e_args 2))  (* msgid *)
    | "SYS_PP_REQ_BEGIN"  -> SkipEvent
    | "SYS_PP_REQ_END"    -> SkipEvent
    | "SYS_PP_RESP_BEGIN" -> SkipEvent
    | "SYS_PP_RESP_END"   -> SkipEvent

    | "SYS_FATAL"         -> TodoEvent("fatal")
    | "SYS_FATAL_LOCK"    -> TodoEvent("fata_lock")
    | "SYS_NOTE"          -> TodoEvent("note")

    | "SYS_EXIT"          -> Exit
    | "SYS_ABORT"         -> Exit
    | "SYS_SHUTDOWN"      -> Exit

    | "SYS_POLL_BEGIN"    -> PollBegin
    | "SYS_POLL"          -> Poll( ev.e_rv = 0, nth ev.e_args 2)
	(* if note = "real" then SkipEvent else *)

    | "SYS_SELECT_BEGIN"  -> PollBegin
    | "SYS_SELECT"        -> Poll( ev.e_rv = 0, nth ev.e_args 1)

    (* Skip page fault events for now *)
    | "SYS_PAGEFAULT_BEGIN" -> SkipEvent
    | "SYS_PAGEFAULT_END" -> SkipEvent

    | _ -> raise (Unmatched_event ev.e_sc_str)


(* **************************************************************** *)
(* Logfile Parsing                                                  *)
(* core: short sc / le: num_sa, sas                                 *)
(* **************************************************************** *)

let get_int64 ctype =
  match ctype with
    | Swig.C_int64 i -> i
    | _ -> raise (Parse_error "get_int64")
;;
 
let event_of_logentry le =
  let fz   = Bridge._logentry_thaw_t_fz_get_f [le] in
(*   let e_s  = (nth (Bridge._logentry_frozen_t_str_len_get_f(fz)) 0) in *)
(*   let e = 1 + (match e_s with Swig.C_short i -> (Swig.get_int e_s) | _ -> 5) in *)
  let strl = Swig.get_int (nth (Bridge._logentry_frozen_t_str_len_get_f(fz)) 0) in
  let sarg = (if strl > 0 then
      (Swig.get_string (nth (Bridge._logentry_thaw_t_str_get_f [le]) 0)) else "") in
  let sc   = Swig.get_int (nth (Bridge._logentry_frozen_t_sc_get_f fz) 0) in
  let rv   = Swig.get_int (nth (Bridge._logentry_frozen_t_rv_get_f fz) 0) in
  let pid  = Swig.get_int (Bridge._binlog_mkpid le) in
  let tid  = get_int64    (Bridge._binlog_mktid le) in
  let args = [ (Swig.get_int (Bridge._binlog_arg1 le));
	       (Swig.get_int (Bridge._binlog_arg2 le));
	       (Swig.get_int (Bridge._binlog_arg3 le)) ] in
    {
      e_eid    = 123456;
      e_pid    = pid;
      e_tid    = tid;
      e_ts     = (get_int64 (Bridge._binlog_mkts le));
      e_rv     = rv;
      e_sc_str = (Syscalls.sc_str_of_int sc);
      e_prog   = (Printf.sprintf "%c" (Swig.get_char (nth (Bridge._logentry_frozen_t_nm_get_f fz) 0)));
      e_args   = args;
      e_str    = sarg;
      e_ptid   = (if (Syscalls.sc_str_of_int sc) = "SYS_EXEC_CHILD" || (Syscalls.sc_str_of_int sc) = "SYS_FORK"
	then (Int64.of_string sarg) else (Int64.of_int 0));
    }
;;

let string_of_stream s =
  Printf.sprintf "[pid=%d|tid=%Ld|ts=%Ld|%s|%s(%d,%d,%d)->%d|str=%s|ptid=%Ld]"
    s.s_ev.e_pid
    s.s_ev.e_tid
    s.s_ev.e_ts
    s.s_ev.e_prog
    s.s_ev.e_sc_str
    (nth s.s_ev.e_args 0)
    (nth s.s_ev.e_args 1)
    (nth s.s_ev.e_args 2)
    s.s_ev.e_rv
    s.s_ev.e_str
    s.s_ev.e_ptid
;;


let stream_of_logentry le =
  try
    let ev = (event_of_logentry le) in
      { s_ev=ev; s_type=(mk_eventtype ev)}
  with 
    | Failure(e) ->
	print_endline "Troublesome line.";
	raise (Parse_error "stream_of_logentry: failure:")
    | Not_found ->
	print_endline "nf";
	raise (Parse_error "stream_of_logentry: not found")
;;

let stream_of_dbrow res tupnum =
  try
  let ev = {
      e_eid   =(int_of_string (res#getvalue tupnum (res#fnumber "eid")));
      e_pid   =(int_of_string (res#getvalue tupnum (res#fnumber "pid")));
      e_tid   =(Int64.of_string (res#getvalue tupnum (res#fnumber "tid")));
      e_ptid  =(Int64.of_string (res#getvalue tupnum (res#fnumber "ptid")));
      e_ts    =(Int64.of_string (res#getvalue tupnum (res#fnumber "ts")));
      e_sc_str=(res#getvalue tupnum (res#fnumber "sc"));
      e_rv    =(int_of_string (res#getvalue tupnum (res#fnumber "rv")));
      e_prog  =(res#getvalue tupnum (res#fnumber "prog"));
      e_args  =[(int_of_string (res#getvalue tupnum (res#fnumber "arg1")));
		(int_of_string (res#getvalue tupnum (res#fnumber "arg2")));
		(int_of_string (res#getvalue tupnum (res#fnumber "arg3")))];
      e_str   =(res#getvalue tupnum (res#fnumber "str"));
    } in
	{ s_ev=ev; s_type=(mk_eventtype ev) }
  with
    | Not_found ->
	raise (Parse_error "stream_of_dbrow")
;;

(* **************************************************************** *)
(* Parse and load into DB                                           *)
(* parse: binlog -> db_conn -> unit                                 *)
(* **************************************************************** *)

open Swig
open Bridge

let store_event_db ev eid =
    let sql = Printf.sprintf
      (* . . . . . . . . . . . . . . . . . . . . . . . . . . . *)"
      INSERT INTO ev (eid,pid,tid,ptid, ts,prog,  sc,rv,arg1,arg2,arg3, str)
      VALUES         ( %d, %d,%Ld, %Ld,%Ld,'%s','%s',%d,  %d,  %d,  %d,'%s');"
                      eid ev.e_pid
                              ev.e_tid
                                  ev.e_ptid
                                        ev.e_ts
		                           ev.e_prog
		                                ev.e_sc_str
		                                     ev.e_rv
		                                       (nth ev.e_args 0)
		                                       (nth ev.e_args 1)
		                                       (nth ev.e_args 2)
		                                       (Postgresql.escape_string ev.e_str)
      (* . . . . . . . . . . . . . . . . . . . . . . . . . . . *) in
      (*Printf.printf "SQL: %s\n" sql;*)
      let res = (conn#exec sql) in
	match res#status with 
	  | Command_ok -> ()
	  | Tuples_ok -> ()
	  | _ -> Printf.printf "ERROR: %s\n" (res#error)
;;

let store_event_out ev eid out =
  try
    ignore (Str.search_forward (Str.regexp "ADDRESS") ev.e_str 0)
  with
    | Not_found ->

  let str = (Postgresql.escape_string
		(Str.global_replace (Str.regexp "\n") "_NEWLINE_" ev.e_str)) in
  let str2 = (Postgresql.escape_string
		(Str.global_replace (Str.regexp "\r") "_NEWLINER_" str)) in

  Printf.fprintf out
    (* . . . . . . . . . . . . . . . . . . . . . . . . . . . *)
    "%d|%Ld|%Ld|%Ld|%s|%s|%d|%d|%d|%d|%s\n"
    ev.e_pid
            ev.e_tid
                ev.e_ptid
                      ev.e_ts
		         ev.e_prog
		              ev.e_sc_str
		                   ev.e_rv
		                     (nth ev.e_args 0)
		                     (nth ev.e_args 1)
		                     (nth ev.e_args 2)
                (if (String.length str2) > 149 then
		    (String.sub str2 0 149)
		  else str2)
      (* . . . . . . . . . . . . . . . . . . . . . . . . . . . *)
;;

let rec parse binlog eid evout =
  let eof = _binlog_eof binlog in
    match eof with
      | C_enum `isNotEof ->
	  let le = _binlog_next binlog in
	  let stream = (stream_of_logentry le) in
	    store_event_out stream.s_ev eid evout;
	    parse binlog (eid+1) evout

      | C_enum `isEof ->
	  eid
      | C_enum `Int(i) ->
	  print_endline "parse: reached Int(i)";
	  -1
      | _ ->
	  print_endline "parse: reached strange pattern";
	  -1

;;
