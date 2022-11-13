#pragma once

#include <cstdint>
#include <ctime>

#include "common/util/Assert.h"

/*!
 * Timer for measuring time elapsed with clock_monotonic
 */
class Timer {
 public:
  /*!
   * Construct and start timer
   */
  explicit Timer() { start(); }

#ifdef _WIN32
  int clock_gettime_monotonic(struct timespec* tv) const;
#endif

  void clock_get_platform_timer(struct timespec* tv) const;

  /*!
   * Start the timer
   */
  void start(bool is_tas_timer = true);

  /*!
   * Get nanoseconds elapsed
   */
  int64_t getNs() const;

  /*!
   * Get microseconds elapsed
   */
  double getUs() const { return (double)getNs() / 1.e3; }

  /*!
   * Get milliseconds elapsed
   */
  double getMs() const { return (double)getNs() / 1.e6; }

  /*!
   * Get seconds elapsed
   */
  double getSeconds() const { return (double)getNs() / 1.e9; }

  bool _is_tas_timer = false;
  struct timespec _startTime = {};
};
