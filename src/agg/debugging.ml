let blog = Bridge._binlog_open(Swig.C_string "/tmp/log-unnamed-beC8l");;

let eof = Bridge._binlog_eof blog;;

let le = Bridge._binlog_next blog;;

let fz   = Bridge._logentry_thaw_t_fz_get_f [le];;

let e_s  = (nth (Bridge._logentry_frozen_t_str_len_get_f(fz)) 0);;

let e = 1 + (match e_s with Swig.C_short i -> (Swig.get_int e_s) | _ -> 5);;
