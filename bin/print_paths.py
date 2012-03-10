#!/usr/bin/env python

import pg

DBNAME = 'debug'
HOST = 'localhost'
USER = 'debug'
PASSWD = 'debug'

conn = pg.connect(dbname = DBNAME, host = HOST, user = USER, passwd = PASSWD)
dgn_res = conn.query('SELECT pid, rid, eid FROM dgn WHERE rid != -1')
ev_res = conn.query('SELECT eid, prog, sc, rv, arg1, arg2, arg3, str FROM ev')

# print 'Content-type: text/html'
# print
print '<html><body>'

prev = None
dgn_list = dgn_res.getresult()
ev_list = ev_res.getresult()
for i in dgn_list:
    ev = filter(lambda x: x[0] == i[2] and True or False, ev_list)[0]
    if prev != None and i[1] != prev[1]:
        print '<br />'

    if prev != None and i[1] == prev[1] and i[0] != prev[0]:
        print '<b>%d. pid=%d prog=%c %s (%d, %d, %d)=>%d [%s]</b><br />' % (i[1],
                                                                            i[0],
                                                                            ev[1],
                                                                            ev[2],
                                                                            ev[4],
                                                                            ev[5],
                                                                            ev[6],
                                                                            ev[3],
                                                                            ev[7])
    else:
        print '%d. pid=%d prog=%c %s (%d, %d, %d)=>%d [%s]<br />' % (i[1],
                                                                     i[0],
                                                                     ev[1],
                                                                     ev[2],
                                                                     ev[4],
                                                                     ev[5],
                                                                     ev[6],
                                                                     ev[3],
                                                                     ev[7])
    prev = i[:2]

conn.close()

print '</body></html>'
