#ifndef COMM_INCLUDED
#define COMM_INCLUDED
#include "server.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#ifndef INADDR_NONE
#define INADDR_NONE (-1)
#endif
typedef int SOCKET;
#define SOCKET_ERROR (-1)
extern int close(int);
#define closesocket(x) close(x)
#endif

extern int trace;
extern const char *identity;

extern const char *mesgname(Header_T msg);
extern void tracemesg(const char *fmt, ...);
extern void sendmesg(SOCKET s, const void *buf, int size);
extern void recvmesg(SOCKET s, void *buf, int size);

#endif
