#include "NanoOs.h"
#include "NanoOsLibC.h"

/// @fn int timespec_get(struct timespec* spec, int base)
///
/// @brief Get the current time in the form of a struct timespec.
///
/// @param spec A pointer to the sturct timespec to populate.
/// @param base The base for the time (TIME_UTC).
///
/// @return Returns teh value of base on success, 0 on failure.
int timespec_get(struct timespec* spec, int base) {
  if (spec == NULL) {
    return 0;
  }
  
  unsigned long now = getElapsedMilliseconds(0);
  spec->tv_sec = (time_t) (now / 1000UL);
  spec->tv_nsec = now * 1000000UL;

  return base;
}

