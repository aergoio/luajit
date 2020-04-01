#include <stdio.h>
#include <inttypes.h>
#ifdef _MSC_VER
# include <Windows.h>
#else
# include <time.h>
#endif
#include <errno.h>
#include "lj_bc.h"

double lj_nanosec(void)
{
#ifdef _MSC_VER
  static LARGE_INTEGER frequency;
  if (frequency.QuadPart == 0)
    ::QueryPerformanceFrequency(&frequency);
  LAREGE_INTEGER now;
  ::QueryPerformanceCounter(&now);
  return now.QuadPart / double(frequency.QuadPart);
#elif defined(__MINGW32__) || defined(__MINGW64__)
  return 0.0;
#else
  int rc;
  struct timespec now;
  rc = clock_gettime(CLOCK_MONOTONIC, &now);
  if (rc != 0) {
    printf("ec: %s\n", strerror(errno));
    exit(errno);
  }
  return now.tv_sec + now.tv_nsec * 1.0e-9;
#endif
}

typedef struct M {
  char *name;
  double totalTime;
  uint64_t count;
} M;

#define BCM(name, ma, mb, mc, mt, gas)	{ #name, 0, 0 },
M BCOpM[] = {
BCDEF(BCM)
};
#undef BCM

int bOp = BC__MAX;
double start;

void lj_measure_start(int op)
{
  double end = lj_nanosec();
  if (bOp != BC__MAX) {
    BCOpM[bOp].totalTime += (end - start);
  }
  bOp = op;
  start = end;
  BCOpM[op].count++;
}

void lj_measure_output(void)
{
  int i;
  for (i = 0; i < BC__MAX; i++) {
    if (BCOpM[i].count > 0) {
	  printf("OP: %2d, NAME: %-6s, COUNT: %6" PRIu64 ", TOTAL TIME: %3.9lf, AVG TIME: %3.9lf\n", 
             i, BCOpM[i].name, BCOpM[i].count, BCOpM[i].totalTime, BCOpM[i].totalTime / BCOpM[i].count);
	}
  }
}
