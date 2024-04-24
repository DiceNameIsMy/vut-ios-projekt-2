#ifndef DBG_H
#define DBG_H

#include <stdio.h>

#ifdef DEBUG
#define loginfo( s, ... ) \
    fprintf( stderr, "[INF] pid:%i |" __FILE__ ":%u: " s "\n", getpid(), __LINE__, ##__VA_ARGS__ )
#else
#define loginfo( s, ... )
#endif

#endif