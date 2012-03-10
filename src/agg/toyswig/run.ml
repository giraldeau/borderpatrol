open Swig
open Bridge

let parse_le le =
  let i1 = _logentry_t_i1__get___f [le] in
  Printf.printf "i1: %d \n" (get_int (List.hd i1));
  let core = _logentry_t_core__get___f [le] in
  let rv = _logentry_core_t_rv__get___f core in
    Printf.printf "rv: %d \n" (get_int (List.hd rv));
;;

let _ =
  let x = C_int 3 in
  let y = _expand(x) in
  let z = _expand(y) in
  let a = _expand(z) in
  let r = _open_stream(C_string "/home/ejk/di.raw") in
  let le = _get_logentry(r) in
    parse_le le

(*
  let sc = logentry_t_i1_get le in
    _free_logentry(le);
    _close_stream(r)
*)

(*
# open Swig;;
# let x = C_int 3;;
val x : 'a Swig.c_obj_t = C_int 3
# let y = _expand(x);;
expand(3) called in C
val y : Bridge.c_obj = C_int 4
*)
