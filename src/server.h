/* <server.h>=                                                              */
#ifndef SERVER_INCLUDED
#define SERVER_INCLUDED
#include "nub.h"

typedef enum {
#define xx(name) name,
        /* <message codes>=                                                         */
        xx(NUB_CONTINUE)
        xx(NUB_QUIT)
        xx(NUB_STARTUP)
        xx(NUB_BREAK)
        xx(NUB_FAULT)

        /* <message codes>=                                                         */
        xx(NUB_SET)

        /* <message codes>=                                                         */
        xx(NUB_REMOVE)

        /* <message codes>=                                                         */
        xx(NUB_FETCH)

        /* <message codes>=                                                         */
        xx(NUB_STORE)

        /* <message codes>=                                                         */
        xx(NUB_FRAME)

        /* <message codes>=                                                         */
        xx(NUB_SRC)

#undef xx
} Header_T;

/* <message data structures>=                                               */
struct nub_fetch {
        int space;
        void *address;
        int nbytes;
};

/* <message data structures>=                                               */
struct nub_store {
        int space;
        void *address;
        int nbytes;
        char buf[1024];
};

/* <message data structures>=                                               */
struct nub_frame {
        int n;
        Nub_state_T state;
};


#endif
