
open Events

open Postgresql

let conn = new connection ~conninfo:"dbname=debug" ()

let check_res r =
  match r#status with 
    | Command_ok -> ()
    | Tuples_ok -> ()
    | _ -> Printf.printf "ERROR: %s\n" (r#error)
;;

let abbrev_sql s =
  (Str.global_replace (Str.regexp "\n") "/"
      (if String.length s > 20 then
	  ((String.sub s 0 20) ^ "...")
	else
	  s))
;;    

(* **************************************************************** *)

let rec ch_to_string ch =
  try
    let l = input_line ch in
      l ^ (ch_to_string ch)
  with End_of_file -> ""
;;

let exec_file fn =
  let fin = open_in fn in
  let text = (ch_to_string fin) in
  let sqllist = Str.split (Str.regexp ";") text in
    List.iter (fun sql ->
      Printf.printf "  running sql: %s" (abbrev_sql sql);
      let res = conn#exec sql in
	Printf.printf "  -> %s\n"
	  (match res#status with
	    | Tuples_ok -> "ok"
	    | Command_ok -> "ok"
	    | _ -> Printf.sprintf "ERROR: %s" (res#error))
	      ) sqllist;
    close_in fin
;;

(* **************************************************************** *)

let dump_work sm ctx =
  let cmid = (mid_of_pt sm.s_ev.e_pid sm.s_ev.e_tid) in
  List.iter
    (fun d ->
      let sql = Printf.sprintf
        (* . . . . . . . . . . . . . . . . . . . . . . . . . . . *)"
        INSERT INTO dgn (pid,tid,rid,eid,ts)
        VALUES          ( %d,%Ld, %d, %d,%Ld)"
	d.d_mid.m_pid d.d_mid.m_tid d.d_rid sm.s_ev.e_eid sm.s_ev.e_ts in
	check_res (conn#exec sql)
      (*Printf.printf "dgn: %Ld -> %d\n" d.d_tid d.d_rid*)
    )
    (List.filter (fun d ->  d.d_new || (mideq d.d_mid cmid)) ctx.c_dgn);
;;

(*type msg_t = { msg_pid: pid_t; msg_ev: eid_t; msg_send: bool; msg_prot: int; msg_conn: conn_t; msg_id: msgid_t; msg_rid: rid_t }*)

let dump_msg msgs =
  if List.length msgs = 0 then
    ()
  else
    let msg = (List.hd msgs) in 
    let sql = Printf.sprintf
      (* . . . . . . . . . . . . . . . . . . . . . . . . . . . *)"
        INSERT INTO msg (pid,eid,send,prot,conn,msgid,rid)
        VALUES          ( %d, %d,  %d,'%s','%s',   %d, %d)"
      msg.msg_pid
      msg.msg_ev
      (if msg.msg_send then 1 else 0)
      msg.msg_prot
      msg.msg_conn
      msg.msg_id
      msg.msg_rid in
      (*print_endline sql;*)
      check_res (conn#exec sql)
;;

(* **************************************************************** *)

let copy_ev efn =
  List.iter
    (fun sql ->
      let res = conn#exec sql in
	Printf.printf "  copy_ev sql: %s -> %s\n"
	  (abbrev_sql sql)
	  (match res#status with
	    | Tuples_ok -> "ok"
	    | Command_ok -> "ok"
	    | _ -> Printf.sprintf "ERROR: %s\n" (res#error))
    ) [
	("COPY   ev_tmp (pid,tid,ptid,ts,prog,sc,rv,arg1,arg2,arg3,str)
         FROM '" ^ efn ^ "' WITH DELIMITER '|'");

	("INSERT INTO ev (pid,tid,ptid,ts,prog,sc,rv,arg1,arg2,arg3,str)
          SELECT * FROM ev_tmp ORDER BY ts;")
      ];
;;

(* **************************************************************** *)
(*integer PRIMARY KEY DEFAULT nextval('serial')
CREATE TABLE ev AS (
           SELECT nextval('ev_seq') AS eid, ev_tmp.* FROM ev_tmp ORDER BY ts*)
