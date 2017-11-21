#ifndef TEST_HELPERS_H
#define TEST_HELPERS_H

#include <unistd.h>

#define DO_100_TIMES_WHILE(action, condition) \
{ \
    int counter = 0; \
    while (counter++ < 100) { \
        action; \
        if (!(condition)) \
            break; \
        usleep(100); \
    } \
}



#endif // TEST_HELPERS_H
