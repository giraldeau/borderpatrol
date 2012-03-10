#ifndef	_BP_GETSERVICE_H
#define	_BP_GETSERVICE_H

/* Gets the port number from the services files. If a user specified services
 * file is passed as a parameter, the mappings in the user specified will
 * override the ones in /etc/services. */
int getportbyname(const char *name);

#endif	/* getservice.h */
