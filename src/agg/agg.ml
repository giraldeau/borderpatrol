
open Events
open Join
open Postgresql

(* **************************************************************** *)

let _ =
  let args = Sys.argv in
    if Array.length args < 4 then
      print_endline "usage: agg.ml <log.raw> <events_fn> <work_fn>"
    else
      let base_dir = args.(4) in
      (*Printf.printf "open log: %s\n" args.(1);*)
      let blog = Bridge._binlog_open(Swig.C_string args.(1)) in
      let evfn = args.(1) ^ ".edb" in
      let conn = new connection ~conninfo:"dbname=debug" () in

      (* STEP 1: INIT *)
      let step_init() =
	print_endline "agg: initializing database ...";
	Db.exec_file(base_dir ^ "init.sql")
      in

      (* STEP 2: PARSE BINLOG *)
      let step_parse() = 
	print_endline ("agg: parsing events into database " ^ evfn ^ " ...");
	let evout = open_out evfn in
	let leid = Parse.parse blog 0 evout in
	  Printf.printf " -> parsed %d events.\n" leid;
	  ignore (Bridge._binlog_close(blog));
	  close_out evout
      in

      (* STEP 3: COPY EVENTS TO DB *)
      let step_copy() =
	print_endline ("agg: copying database " ^ evfn ^ " to psql ...");
	Db.copy_ev evfn;
	print_endline " -> done."
      in

      (* STEP 4: TEMPORAL JOIN *)
      let step_join() =
	let ctx = { c_dgn=[]; c_fd=Hashtbl.create 40; c_conn=Hashtbl.create 40; c_msgs=[] } in
	  print_endline "agg: performing temporal join ...";
	  let res = conn#exec ("SELECT * FROM ev ORDER BY eid") in
	  let graphout = (open_out "/tmp/bp.msg") in
	    (*Printf.fprintf graphout "digraph G {\n";*)
	    Join.run res 0 ctx graphout;
	    (*Printf.fprintf graphout "}\n";*)
	    print_endline "agg: running queries ...";
	    Db.exec_file(base_dir ^ "queries.sql");
	    close_out graphout;
	    print_endline "agg: done."
      in

	step_init();
	step_parse();
	step_copy();
	step_join();

	print_endline "TODO : handle_close";
	print_endline "TODO : pth";
;;

(* **************************************************************** *)
