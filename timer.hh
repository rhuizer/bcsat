#ifndef BC_TIMER_HH
#define BC_TIMER_HH

/*
 * Copyright (c) 2008 Tommi Junttila
 * Released under the GNU General Public License version 2.
 */


/**
 * \brief A very simple wrapper class for measuring elapsed user+system time.
 */

class Timer
{
  double start_time;
public:
  /**
   * Create and start a new timer.
   */
  Timer();

  /**
   * Reset the timer.
   */
  void reset();

  /**
   * Get the user+system time (in seconds) elapsed since the creation or
   * the last reset() call of the timer.
   */
  double get_duration();
};


#endif
