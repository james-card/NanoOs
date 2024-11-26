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

/// @fn int printString(const char *string)
///
/// @brief C wrapper around Serial.print for a C string.
///
/// @return This function always returns 0;
int printString(const char *string) {
  Serial.print(string);

  return 0;
}

/// @fn int printInt(int integer)
///
/// @brief C wrapper around Serial.print for an integer.
///
/// @return This function always returns 0.
int printInt(int integer) {
  Serial.print(integer);

  return 0;
}

