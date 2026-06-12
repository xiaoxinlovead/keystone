#ifndef _RUNTIME_H_
#define _RUNTIME_H_

#include <stdint.h>

#define ENABLE_VEC                                                             \
  asm volatile(                                                                \
      "csrs mstatus, %[bits];" ::[bits] "r"(0x00000600 & (0x00000600 >> 1)))

extern int64_t event_trigger;
extern int64_t timer;
extern int64_t time_timer;
// SoC-level CSR
extern uint64_t hw_cnt_en_reg;

#ifdef ARA_LINUX
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <linux/perf_event.h>

static int ara_perf_fd = -1;

static inline int ara_perf_fd_init(void) {
  if (ara_perf_fd < 0) {
    struct perf_event_attr pe = {0};
    pe.size   = sizeof(pe);
    pe.type   = PERF_TYPE_SOFTWARE;
    pe.config = PERF_COUNT_SW_CPU_CLOCK;
    pe.disabled = 0;
    ara_perf_fd = syscall(__NR_perf_event_open, &pe, 0, -1, -1, 0);
    printf("[perf] perf_event_open fd=%d errno=%d\n", ara_perf_fd,
           ara_perf_fd < 0 ? errno : 0);
  }
  return ara_perf_fd;
}

static inline int64_t get_cycle_count(void) {
  if (ara_perf_fd_init() < 0) return 0;
  long long cnt = 0;
  read(ara_perf_fd, &cnt, sizeof(cnt));
  return cnt;
}

static inline int64_t get_time_count(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return ts.tv_sec * 1000000000LL + ts.tv_nsec;
}
#else
// Return the current value of the cycle counter
inline int64_t get_cycle_count() {
  int64_t cycle_count;
  asm volatile("fence; csrr %[cycle_count], cycle"
               : [cycle_count] "=r"(cycle_count));
  return cycle_count;
};

// Return the current value of the time counter
inline int64_t get_time_count() {
  int64_t tc;
  asm volatile("csrr %[tc], time" : [tc] "=r"(tc));
  return tc;
};

#endif

#ifndef SPIKE
// Enable and disable the hw-counter
#define HW_CNT_READY hw_cnt_en_reg = 1;
#define HW_CNT_NOT_READY hw_cnt_en_reg = 0;
// Start and stop the cycle counter
static inline void start_timer() { timer = -get_cycle_count(); }
static inline void stop_timer() { timer += get_cycle_count(); }
static inline int64_t get_timer() { return timer; }
// Start and stop the time counter
static inline void start_timer_time() { time_timer = -get_time_count(); }
static inline void stop_timer_time() { time_timer += get_time_count(); }
static inline int64_t get_timer_time() { return time_timer; }
#else
#define HW_CNT_READY ;
#define HW_CNT_NOT_READY ;
static inline void start_timer() { while (0); }
static inline void stop_timer() { while (0); }
static inline int64_t get_timer() { return 0; }
static inline void start_timer_time() { while (0); }
static inline void stop_timer_time() { while (0); }
static inline int64_t get_timer_time() { return 0; }
#endif

#endif // _RUNTIME_H_
