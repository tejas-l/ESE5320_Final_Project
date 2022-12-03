#ifndef _LOGGER_H
#define _LOGGER_H

#include <stdio.h>


#define LOG_DEBUG 4

#define LOG_INFO_2 3

#define LOG_INFO_1 2

#define LOG_CRIT 1

#define LOG_ERR 0

#define LOG_NO_PRINT -1 // never useee!!!

#define LOG_LEVEL LOG_NO_PRINT


/* logger function */
// inline void LOG(int level, const char *format, ...){

// #if(level <= LOG_LEVEL)
//     va_list args;
//     va_start (args, format);
//     vprintf (format, args);
//     va_end (args);
// #else
//     printf("\nyo???????\n");
// #endif
// }

#if(LOG_LEVEL != LOG_NO_PRINT)

#define LOG(LEVEL, ...) \
if((LEVEL) <= LOG_LEVEL) {\
    printf(__VA_ARGS__); \
}

#else

#define LOG(LEVEL, ...) 

#endif


#endif