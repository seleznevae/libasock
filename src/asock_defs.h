#ifndef ASOCK_DEFS_H
#define ASOCK_DEFS_H

#define EPOLL_WAIT_DEFAULT_TIMEOUT 10 /* in milliseconds*/

enum ServiceStatus {
    Init,
    Configuration,
    Aborted,
    Running
};

enum DirectiveType {
    DirectNone,
    DirectStart,
    DirectStop,
    DirectBind,
    DirectCreateAsock,
    DirectSetns,
};


#endif /* ASOCK_DEFS_H */
