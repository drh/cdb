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
typedef int SOCKET;
#define SOCKET_ERROR (-1)
#define closesocket close
#endif

extern int trace;
extern const char *identity;

extern const char *msgname(Header_T msg);
extern void tracemsg(const char *fmt, ...);
extern void sendmsg(SOCKET s, const void *buf, int size);
extern void recvmsg(SOCKET s, void *buf, int size);

#endif
