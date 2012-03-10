
open Events
open Parse

exception Unknown_syscall
exception Unskipped_event
exception Join_error

let debug_out = ref false

let paranoid_mode = ref false

let fake_msg_id = ~-9

(* ********************************************************************** *)

type gensym_t = { mutable gs: int }
let gensym_ct = { gs=0 };;
let gensym() =
  gensym_ct.gs <- gensym_ct.gs + 1;
  gensym_ct.gs;;

type genrid_t = { mutable gs: int }
let genrid_ct = { gs=0 };;
let genrid() =
  genrid_ct.gs <- genrid_ct.gs + 1;
  genrid_ct.gs;;

let genstring s =
  Printf.sprintf "%s-%d" s (gensym())
;;

let new_simple_stream() =
  { c_id=GenSym(gensym()) }
;;

(* ********************************************************************** *)
(* Context API *)

let conn_of_fd ctx pid fd =
  try
    Hashtbl.find ctx.c_fd (pid,fd)
  with
    | Not_found ->
	if !paranoid_mode then
	  begin Printf.printf "conn_of_fd (pid=%d,fd=%d) not found.\n" pid fd; raise Join_error end
	else
	  new_simple_stream()
;;

let set_fd_conn ctx pid fd conn =
  Hashtbl.replace ctx.c_fd (pid,fd) conn;
  ctx
;;

let rec rid_of_msg ctx pid fd msgid existrid =
  try 
    Hashtbl.find ctx.c_conn ((conn_of_fd ctx pid fd),msgid)
  with
    | Not_found -> ~-1
	(*if msgid = fake_msg_id then ~-1
	  else rid_of_msg ctx pid fd fake_msg_id existrid*)
;;

let set_msg ctx pid fd msgid rid =
  let connid = (conn_of_fd ctx pid fd) in
    Hashtbl.replace ctx.c_conn (connid,msgid) rid;
    (*Printf.printf "set pid=%d,fd=%d,msg=%d -> "*)
    ctx
;;

let add_msg ctx msg =
  { c_dgn = ctx.c_dgn;
    c_fd = ctx.c_fd;
    c_conn = ctx.c_conn;
    c_msgs = ctx.c_msgs @ [msg];
  }

let dump_ctx ctx =
  Printf.printf "dgn: ";
  List.iter
    (fun dgn -> Printf.printf "%s->%d " (string_of_mid dgn.d_mid) dgn.d_rid) ctx.c_dgn;
  Printf.printf "\nfd: ";
  Hashtbl.iter
    (fun (p,f) c -> Printf.printf "%d.%d->%s " p f (string_of_conn c)) ctx.c_fd;
  Printf.printf "\nconn: ";
  Hashtbl.iter
    (fun (c,m) r -> Printf.printf "%s.%d->%d " (string_of_conn c) m r) ctx.c_conn;
  Printf.printf "\n";
;;

let ___test_ctx() =
  let ctx = { c_dgn=[]; c_fd=Hashtbl.create 40; c_conn=Hashtbl.create 40; c_msgs=[]; } in
  let ctx' = (set_fd_conn ctx 4 2 {c_id=SunPath("ejk")}) in
  let ctx'' = set_msg ctx' 4 2 1 9 in
    dump_ctx ctx''
;;

let distinguished_conn conn =
  match conn.c_id with
    | HostPort(rdv) ->
	(matchportbyname "www" rdv)
(* Printf.printf "HP: %s \n" rdv; begin (Printf.printf "YAYAYA!\n"); true end) *)
	  (*Printf.printf "HP: %s -> %s\n" rdv (if m then "YES" else "NO");*)
    | _ -> false
;;

(* ********************************************************************** *)

let clear_flags ctx =
  { c_dgn = List.map (fun d -> {d_mid=d.d_mid;d_rid=d.d_rid;d_new=false}) ctx.c_dgn;
    c_fd = ctx.c_fd;
    c_conn = ctx.c_conn;
    c_msgs = [] }
;;

let raise_flag ctx mid =
  { c_dgn = 
      List.map (fun d ->
	if mideq d.d_mid mid then
	  { d_mid=mid;d_rid=d.d_rid;d_new=true }
	else
	  d) ctx.c_dgn;
    c_fd = ctx.c_fd;
    c_conn = ctx.c_conn;
    c_msgs = ctx.c_msgs
  }
;;

let dgn_set ctx mid rid =
  let newdgn = { d_mid=mid; d_rid=rid; d_new=true } in
    {
      c_dgn =
	(if List.exists (fun dgn -> mideq dgn.d_mid mid) ctx.c_dgn then
	    List.map
	      (fun dgn ->
		if mideq dgn.d_mid mid then
		  newdgn
		else
		  dgn) ctx.c_dgn
	  else
	    ctx.c_dgn @ [newdgn]);
      c_fd = ctx.c_fd;
      c_conn = ctx.c_conn;
      c_msgs = ctx.c_msgs
    }
;;

let dgn_get ctx mid =
  try 
    let dgn = List.find (fun dgn -> (mideq dgn.d_mid mid) && dgn.d_rid <> ~-1) ctx.c_dgn in
      dgn.d_rid
  with Not_found -> ~-1
;;

(*let dgn_clear dgnl mid =
  List.filter (fun d -> not (mideq d.d_mid mid)) dgnl
;;*)  

(*
let ctx_dgn_clear ctx mid =
  let altered_dgn = dgn_clear ctx.c_dgn mid in
    { c_dgn = altered_dgn;
      c_rusage = ctx.c_rusage;
      c_delay = ctx.c_delay }
;;*)  

(* ********************************************************************** *)
(* stream_join helper functions                                           *)
(*                                                                        *)
(*                                                                        *)
(* ********************************************************************** *)

let ht_keylist ht =
  Hashtbl.fold (fun k v acc -> k::acc) ht []
;;

let handle_fork ctx pmid mid =
  (* find all the ht entries for the parent *)
  let pfc_list = Hashtbl.fold (fun (p,f) v acc ->
    (if p = pmid.m_pid then (p,f,v)::acc else acc)) ctx.c_fd [] in
    (* duplicate parent's fds to child *)
    List.iter (fun (p,f,c) ->
      Hashtbl.add ctx.c_fd (mid.m_pid,f) c) pfc_list;
    (* duplicate parent's dgn to self *)
    (dgn_set ctx mid (dgn_get ctx pmid))
;;

let handle_close ctx cfd mid rid =
  ctx
;;

let replace_all_conn ctx conn conn' =
  let keylist = (ht_keylist ctx.c_fd) in
  (* make (pid/fd)->conn' *)
  let pflist =
    List.filter (fun (p,f) -> (conn_eq (conn_of_fd ctx p f) conn)) keylist in
    
    List.iter (fun pf -> Hashtbl.replace ctx.c_fd pf conn') pflist;

  (* make (conn'/msg)->rid *)
    let cmlist =
      List.filter (fun (c,m) -> (conn_eq c conn)) (ht_keylist ctx.c_conn) in
      List.iter (fun (c,m) -> 
	let rid = Hashtbl.find ctx.c_conn (c,m) in
	  Hashtbl.remove ctx.c_conn (c,m);
	  Hashtbl.add    ctx.c_conn (conn',m) rid) cmlist;
      ctx
;;

(* ********************************************************************** *)
(* stream_join : stream context -> context * tid_t list                   *)
(*   takes an event and current context,                                  *)
(*   returns an updated context and a list of tid's whose dgn changed     *)
(* ********************************************************************** *)

let stream_join stream ctx graphout =
  let pid = stream.s_ev.e_pid in
  let tid = stream.s_ev.e_tid in
  let mid = {m_pid=pid;m_tid=tid} in
  let rid = (dgn_get ctx mid) in
    match stream.s_type with
      | Start(newpid,t) -> ctx

      | Fork(ppid,ptid) ->
	  let pmid = {m_pid=ppid;m_tid=ptid} in
	    (*Printf.printf "P:%s   C:%s\n" (string_of_mid pmid) (string_of_mid mid) ;*)
	    (handle_fork ctx pmid mid)

      | Exec(ptid) -> 
	  (* set all designations to me *)
	  let pmid = {m_pid=pid;m_tid=ptid} in
	    { c_dgn =
		List.map (fun d ->
		  if d.d_mid.m_pid = pid then
		    { d_mid=mid; d_rid=d.d_rid; d_new=true } 
		  else
		    d) ctx.c_dgn;
	      c_fd = ctx.c_fd;
	      c_conn = ctx.c_conn;
	      c_msgs = ctx.c_msgs }

      | PollBegin -> ctx (* (dgn_set ctx mid ~-1) *)

      | Poll(true,fd) -> ctx (*ctx_dgn_clear ctx mid*)

      | Poll(false,fd) ->
	  (dgn_set ctx mid
	      (rid_of_msg ctx pid fd fake_msg_id rid))

      (* *** NEW RESOURCES *** *)
      | Open(nfd) ->
	  (* create a new connection, and set it to be the current rid *)
	  (set_msg 
	      (set_fd_conn ctx pid nfd (new_simple_stream()))
	      pid nfd fake_msg_id (dgn_get ctx mid))

      | Socket(fd) ->
	  (* for now, just pretend it's a SimplFile. when we call connect(),
	     we'll make it a bonafied Socket *)
	  (set_msg 
	      (set_fd_conn ctx pid fd (new_simple_stream()))
	      pid fd fake_msg_id (dgn_get ctx mid))

      | Pipe(fi,fo) ->
	  (* for now, just pretend it's a SimplFile. when we call connect(),
	     we'll make it a bonafied Socket *)
	  (set_msg
	      (set_msg 
		  (set_fd_conn
		      (set_fd_conn
			  ctx pid fi (new_simple_stream()))
		      pid fo (new_simple_stream()))
		  pid fi fake_msg_id (dgn_get ctx mid))
	      pid fo fake_msg_id (dgn_get ctx mid))

      | Dup(oldfd,newfd) ->
	  (* reuse the same connection with the new fd *)
	  (set_fd_conn ctx pid newfd
	      (conn_of_fd ctx pid oldfd))

      | Close(cfd) ->
	  (handle_close ctx cfd mid rid)

      | Accept(nfd,rdv) ->
	  (* create another SimpleStream connection *)
	  (set_fd_conn ctx pid nfd { c_id=HostPort(rdv) })

      | Connect(fd,rdv) ->
	  let conn = conn_of_fd ctx pid fd in
	  let conn' = { c_id=HostPort(rdv) } in
	    (replace_all_conn ctx conn conn')

      (* *** IO OPERATIONS *** *)
      (* todo : make sure current rid == rid in rusage *)
      | WriteBegin(fd) -> ctx
	  (*set_msg ctx pid fd fake_msg_id rid*)
      | Write(fd) -> ctx
      | ReadBegin(fd) -> ctx (* (dgn_set set_msg ctx pid fd fake_msg_id ~-56) *)
      | Read(fd) ->
	  let nrid = (rid_of_msg ctx pid fd fake_msg_id rid) in
	  if !debug_out then Printf.printf "readend (pid=%d,ts=%Ld): fd=%d rid=%d\n" pid stream.s_ev.e_ts fd nrid;
	    if nrid = ~-1 then ctx
	    else dgn_set ctx mid nrid

      (* *** PROTOCOL PROCESSORS *** *)
      | ProtocolInit(fd) ->
	  let conn = (conn_of_fd ctx pid fd) in
	  let conn' = { c_id=conn.c_id } in
	    (replace_all_conn ctx conn conn')

      (*| RequestBegin(proto,fd) ->
	  (set_msg ctx pid fd fake_msg_id  (genrid()))*)
    
      | MessageSend(proto,fd,msgid) ->
	  Printf.fprintf graphout "%s|send|%d|%s_%s|%s-%d\n" (Int64.to_string stream.s_ev.e_ts) rid stream.s_ev.e_prog (string_of_mid mid) (string_of_conn (conn_of_fd ctx pid fd)) msgid
	  ;
	  (add_msg
	      (set_msg ctx pid fd msgid (dgn_get ctx mid))
	      {msg_pid=pid; msg_ev=stream.s_ev.e_eid; msg_send=true; msg_prot=proto; msg_conn=(string_of_conn (conn_of_fd ctx pid fd)); msg_id=msgid; msg_rid=(dgn_get ctx mid);})

      | MessageRecv(proto,fd,msgid) ->
	  (* set fake_msg_id to point to rid(msgid) *)
	  let conn = (conn_of_fd ctx pid fd) in
	  let arid =
	    if (distinguished_conn conn) then
	      (* Request head. This specifies the starting point of a path. *)
	      begin Printf.printf "REQ!\n"; (genrid()) end
	    else
	      (rid_of_msg ctx pid fd msgid rid) in
	    Printf.fprintf graphout "%s|recv|%d|%s_%s|%s-%d\n" (Int64.to_string stream.s_ev.e_ts) arid stream.s_ev.e_prog (string_of_mid mid) (string_of_conn conn) msgid
	    ;
	    (add_msg 
		(set_msg (dgn_set ctx mid arid) pid fd fake_msg_id arid)
	      {msg_pid=pid; msg_ev=stream.s_ev.e_eid; msg_send=false; msg_prot=proto; msg_conn=(string_of_conn (conn_of_fd ctx pid fd)); msg_id=msgid; msg_rid=(rid_of_msg ctx pid fd msgid rid);})


      | Exit -> (dgn_set ctx mid ~-1)
      | Abort -> (dgn_set ctx mid ~-1)
      | Shutdown -> (dgn_set ctx mid ~-1)

      | TodoEvent(s) -> ctx (*Printf.printf "TODO Event : %s\n" s; ctx*)

      | SkipEvent -> ctx
      | Unknown -> raise Unknown_syscall
;;

(* 3082012832|16928|1530306,3249034711|SYS_CONNECT_BEGIN|0|ap|arg(5,0,0),ptid(0),str(0:),sas(0,) EJK *)

let dump_event eid e_out ev =
  let safe_str = Str.global_replace (Str.regexp "\n") "_NEWLINE_" ev.e_str in
  let mid = {m_pid=ev.e_pid;m_tid=ev.e_tid} in
  Printf.fprintf e_out "%d|%d|%Ld|%s|%Ld|%s|%s|%d|%d|%d|%d|%s\n"
    eid
    ev.e_pid
    ev.e_tid
    (string_of_mid mid)
    ev.e_ts
    ev.e_prog
    ev.e_sc_str
    ev.e_rv
    (List.nth ev.e_args 0)
    (List.nth ev.e_args 1)
    (List.nth ev.e_args 2)
    safe_str
;;

(* ********************************************************************** *)

let filter_relevant ctx pid tid =
  ctx
;;

(* ********************************************************************** *)

let rec run res tupnum ctx graphout =
  if tupnum = (res#ntuples) then
    ()
  else
    let stream = (Parse.stream_of_dbrow res tupnum) in
      if !debug_out then print_endline (string_of_stream stream) else ();

      (* ------------------------------------------------------ *)
      let new_ctx = stream_join stream ctx graphout in     (* join *)
	if !debug_out then (dump_ctx (filter_relevant new_ctx stream.s_ev.e_pid stream.s_ev.e_tid)) else ();
	Db.dump_work stream new_ctx;              (* write db *)
	Db.dump_msg new_ctx.c_msgs;
	if (stream.s_ev.e_eid mod 1000) = 0 then
	  Printf.printf "eid: %d\n" stream.s_ev.e_eid
	else ();
	run res (tupnum+1) (clear_flags new_ctx) graphout (* recurr *)
      (* ------------------------------------------------------ *)

;;

(* ********************************************************************** *)

(* 
INSERT INTO events (eid,logdescr) VALUES ()
INSERT INTO association (pid,rid,ts,eid) VALUES ()
*)
