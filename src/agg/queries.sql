
DROP TABLE requests;
CREATE TABLE requests AS (
	SELECT DISTINCT(dgn.rid),ev.str,ev.ts
	FROM dgn
	LEFT JOIN ev ON (dgn.eid=ev.eid)
	WHERE ev.sc = 'SYS_PP_PROP' AND ev.str LIKE 'URL=-=%'
	ORDER BY ev.ts
);

DROP TYPE rid_details CASCADE;
CREATE TYPE rid_details AS (pid int, tid bigint, eid int, prog varchar(5),
	sc varchar(25), rv smallint, str varchar(150));

DROP FUNCTION show_rid(integer);
CREATE FUNCTION show_rid(integer)
RETURNS SETOF rid_details AS
	'SELECT dgn.pid,dgn.tid,dgn.eid,prog,sc,rv,str
	FROM dgn LEFT JOIN ev ON (dgn.eid=ev.eid) WHERE dgn.rid = $1'
LANGUAGE SQL;

DROP TYPE pid_details CASCADE;
CREATE TYPE pid_details AS (rid int, tid bigint, eid int, prog varchar(5),
	sc varchar(25), rv smallint, str varchar(150));

DROP FUNCTION show_pid(integer);
CREATE FUNCTION show_pid(integer)
RETURNS SETOF pid_details AS
	'SELECT dgn.rid,ev.tid,ev.eid,prog,sc,rv,str
	FROM ev LEFT JOIN dgn ON (dgn.eid=ev.eid) WHERE ev.pid = $1 ORDER BY ev.ts'
LANGUAGE SQL;
