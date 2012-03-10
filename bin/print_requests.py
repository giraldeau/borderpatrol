#!/usr/bin/env python

import pg

DBNAME = 'debug'
HOST = 'localhost'
USER = 'debug'
PASSWD = 'debug'

conn = pg.connect(dbname = DBNAME, host = HOST, user = USER, passwd = PASSWD)
res = conn.query('SELECT rid, str FROM requests')

# print 'Content-type: text/html'
# print
print '<html><body>'

for request in res.getresult():
    print '%d. %s<br />' % (request[0], request[1])

conn.close()

print '</body></html>'
