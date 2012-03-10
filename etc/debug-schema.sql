
/* ******************************************************* */

DROP TABLE protocol CASCADE;
CREATE TABLE protocol (
	prot_id integer NOT NULL PRIMARY KEY,
	prot_nm varchar(15) NOT NULL
);

INSERT INTO protocol (prot_nm, prot_id) VALUES ('PP_NULL_ID',0);
INSERT INTO protocol (prot_nm, prot_id) VALUES ('PP_PERLS_ID',1);
INSERT INTO protocol (prot_nm, prot_id) VALUES ('PP_PERLC_ID',2);
INSERT INTO protocol (prot_nm, prot_id) VALUES ('PP_HTTPS_ID',3);
INSERT INTO protocol (prot_nm, prot_id) VALUES ('PP_HTTPC_ID',4);
INSERT INTO protocol (prot_nm, prot_id) VALUES ('PP_PSQLS_ID',5);
INSERT INTO protocol (prot_nm, prot_id) VALUES ('PP_PSQLC_ID',6);
INSERT INTO protocol (prot_nm, prot_id) VALUES ('PP_FCGIS_ID',7);
INSERT INTO protocol (prot_nm, prot_id) VALUES ('PP_FCGIC_ID',8);
INSERT INTO protocol (prot_nm, prot_id) VALUES ('PP_XS_ID',9);
INSERT INTO protocol (prot_nm, prot_id) VALUES ('PP_XC_ID',10);

/* ******************************************************* */

DROP TABLE requests;
CREATE TABLE requests AS (
	SELECT dgn.rid, ev.eid, ev.pid, ev.tid, ev.ts, ev.prog,
	       ev.sc, ev.arg3 AS req_id, protocol.prot_nm AS protocol, str AS url
	FROM ev
	LEFT JOIN dgn ON (dgn.eid = ev.eid)
	LEFT JOIN protocol ON (ev.arg1 = protocol.prot_id)
	WHERE (sc = 'SYS_PP_REQ_BEGIN')
	ORDER BY protocol.prot_nm, ev.arg3
);

/* ******************************************************* */
/* show events for a given rid */

DROP TYPE rid_details CASCADE;
CREATE TYPE rid_details AS (pid int, tid bigint, eid int, prog varchar(5),
	sc varchar(25), rv smallint, str varchar(150));

DROP FUNCTION show_rid(integer);
CREATE FUNCTION show_rid(integer)
RETURNS SETOF rid_details AS
	'SELECT dgn.pid,dgn.tid,dgn.eid,prog,sc,rv,str
	FROM dgn LEFT JOIN ev ON (dgn.eid=ev.eid) WHERE dgn.rid = $1'
LANGUAGE SQL;

/* ******************************************************* */
/* show events for a given pid */

DROP TYPE pid_details CASCADE;
CREATE TYPE pid_details AS (rid int, tid bigint, eid int, prog varchar(5),
	sc varchar(25), rv smallint, str varchar(150));

DROP FUNCTION show_pid(integer);
CREATE FUNCTION show_pid(integer)
RETURNS SETOF pid_details AS
	'SELECT dgn.rid,ev.tid,ev.eid,prog,sc,rv,str
	FROM ev LEFT JOIN dgn ON (dgn.eid=ev.eid) WHERE ev.pid = $1'
LANGUAGE SQL;

/* ******************************************************* */


/* QUERY: Find fcgi requests
SELECT * FROM ev
WHERE pid = (SELECT pid FROM ev WHERE str = 'fcgis' LIMIT 1)
  AND sc LIKE '%_PP_%'
ORDER BY sc,ts;
*/

/* ******************************************************* */

/*
DROP TABLE vars;
CREATE TABLE vars (
       min_ts	  bigint,
       max_ts	  bigint
);

INSERT INTO vars (min_ts, max_ts)
  VALUES ((SELECT MIN(ts) FROM ev),(SELECT MAX(ts) FROM ev));


UPDATE dgn SET ts = ts - (SELECT min_ts FROM vars);
UPDATE ev  SET ts = ts - (SELECT min_ts FROM vars);
*/
