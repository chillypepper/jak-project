#include "Timer.h"

#include "common/common_types.h"

#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#define MS_PER_SEC 1000ULL  // MS = milliseconds
#define US_PER_MS 1000ULL   // US = microseconds
#define HNS_PER_US 10ULL    // HNS = hundred-nanoseconds (e.g., 1 hns = 100 ns)
#define NS_PER_US 1000ULL

#define HNS_PER_SEC (MS_PER_SEC * US_PER_MS * HNS_PER_US)
#define NS_PER_HNS (100ULL)  // NS = nanoseconds
#define NS_PER_SEC (MS_PER_SEC * US_PER_MS * NS_PER_US)

int Timer::clock_gettime_monotonic(struct timespec* tv) const {
  static LARGE_INTEGER ticksPerSec;
  LARGE_INTEGER ticks;
  double seconds;

  if (!ticksPerSec.QuadPart) {
    QueryPerformanceFrequency(&ticksPerSec);
    if (!ticksPerSec.QuadPart) {
      errno = ENOTSUP;
      return -1;
    }
  }

  QueryPerformanceCounter(&ticks);

  seconds = (double)ticks.QuadPart / (double)ticksPerSec.QuadPart;
  tv->tv_sec = (time_t)seconds;
  tv->tv_nsec = (long)((ULONGLONG)(seconds * NS_PER_SEC) % NS_PER_SEC);

  return 0;
}
#endif

void Timer::clock_get_platform_timer(struct timespec* tv) const {
#ifdef OS_POSIX
  clock_gettime(CLOCK_MONOTONIC, tv);
#elif _WIN32
  clock_gettime_monotonic(tv);
#endif
}

void Timer::start(bool is_tas_timer) {
  _is_tas_timer = is_tas_timer;
  if (is_tas_timer) {
    _startTime = {tv_sec : 0, tv_nsec : 0};
  } else {
    clock_get_platform_timer(&_startTime);
  }
}

int64_t Timer::getNs() const {
  if (_is_tas_timer) {
    return 16666667;  // Roughly 1/60th of a second
  } else {
    struct timespec now = {};
    clock_get_platform_timer(&now);
    return (int64_t)(now.tv_nsec - _startTime.tv_nsec) + 1.e9 * (now.tv_sec - _startTime.tv_sec);
  }
}
