
DROP TABLE dgn_ranges;

CREATE TABLE dgn_ranges AS (

SELECT pid,tid,rid,MIN(tsa) AS tsa, tsb
FROM (
	SELECT pid,tid,rid,tsa,MIN(tsb) AS tsb
	FROM (
		SELECT R1.pid, R1.tid, R1.rid, R1.eid, R1.ts AS tsa, R2.ts AS tsb
		FROM dgn AS R1
		JOIN dgn AS R2
		ON (R1.pid = R2.pid
		    AND R1.tid = R2.tid
		    AND R1.rid <> R2.rid /* cuts down the powerset */
		    AND R1.ts < R2.ts)
	) AS R3
	GROUP BY pid,tid,rid,tsa
) AS R4
GROUP BY pid,tid,rid,tsb

);
