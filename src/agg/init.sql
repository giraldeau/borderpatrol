
/* Set up tables for the temporal join */

/* ******************************************************* */

DROP TABLE ev;
DROP TABLE ev_tmp;
CREATE TABLE ev_tmp (
       /* eid   integer NOT NULL PRIMARY KEY, */
       pid   integer NOT NULL,
       tid   bigint NOT NULL,
       ptid  bigint NOT NULL,
       ts    bigint NOT NULL,
       prog  varchar(5) NOT NULL,
       sc    varchar(30) NOT NULL,
       rv    smallint NOT NULL,
       arg1  integer NOT NULL,
       arg2  integer NOT NULL,
       arg3  integer NOT NULL,
       str   varchar(150) NOT NULL
);

DROP SEQUENCE ev_seq;
CREATE SEQUENCE ev_seq;

CREATE TABLE ev (
       eid   integer PRIMARY KEY DEFAULT nextval('ev_seq'),
       pid   integer NOT NULL,
       tid   bigint NOT NULL,
       ptid  bigint NOT NULL,
       ts    bigint NOT NULL,
       prog  varchar(5) NOT NULL,
       sc    varchar(30) NOT NULL,
       rv    smallint NOT NULL,
       arg1  integer NOT NULL,
       arg2  integer NOT NULL,
       arg3  integer NOT NULL,
       str   varchar(150) NOT NULL
);

/* ******************************************************* */

DROP TABLE dgn CASCADE;
CREATE TABLE dgn (
       pid   integer NOT NULL,
       tid   bigint NOT NULL,
       rid   integer NOT NULL,
       eid   integer NOT NULL,
       ts    bigint NOT NULL
);

/* ******************************************************* */

DROP TABLE msg CASCADE;
CREATE TABLE msg (
       pid   integer NOT NULL,
       eid   integer NOT NULL,
       send  smallint NOT NULL,
       prot  VARCHAR(20) NOT NULL,
       conn  VARCHAR(60) NOT NULL,
       msgid integer NOT NULL,
       rid   integer NOT NULL
);

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
INSERT INTO protocol (prot_nm, prot_id) VALUES ('PP_MYSQLS_ID',11);
INSERT INTO protocol (prot_nm, prot_id) VALUES ('PP_MYSQLC_ID',12);

/* ******************************************************* */
