#ifndef NANO_OS_LIB_C
#define NANO_OS_LIB_C

#include <time.h>

#ifdef __cplusplus
extern "C"
{
#endif

// For Coroutines library
#define SINGLE_CORE_COROUTINES
#define COROUTINE_ID_TYPE int8_t

// Missing from Arduino
struct timespec {
  time_t tv_sec;
  long   tv_nsec;
};
#define TIME_UTC 1

int timespec_get(struct timespec* spec, int base);

#ifdef __cplusplus
} // extern "C"
#endif

#endif // NANO_OS_LIB_C
