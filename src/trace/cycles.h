#ifndef CYCLES_H
#define CYCLES_H

#include <stdint.h>

static inline uint64_t
cycle_counter(void) {
  uint64_t cycle;
  __asm__ __volatile__("rdtsc" : "=A"(cycle));
  return cycle;
}

static inline uint32_t
cycle_low(void) {
  return (uint32_t)(cycle_counter() << 32 >> 32);
}

static inline uint32_t
cycle_high(void) {
  return (uint32_t)(cycle_counter() >> 32);
}

// ##########################################################

//#define PROF_ENABLE 1

#ifdef PROF_ENABLE
#define PROF_START(fn) \
  long prof_h_##fn = cycle_high(); \
  long prof_l_##fn = cycle_low()

#define PROF_END(fn) \
  if(cycle_low()-prof_l_##fn > 100000) fprintf(stderr,"***********\n"); \
  fprintf(stderr,"prof|" #fn "|%ld|%ld\n", \
          (cycle_high()-prof_h_##fn), (cycle_low()-prof_l_##fn) )

#else
#define PROF_START(fn) ;
#define PROF_END(fn) ;
#endif

#define CYCLES_NOW(fn) \
  fprintf(stderr,"now|" #fn "|%ld|%ld\n", \
          cycle_high(),cycle_low())

/*  if(cycle_low()-prof_l_##fn > 100000) fprintf(stderr,"***********\n"); \ */

// ##########################################################

#endif
