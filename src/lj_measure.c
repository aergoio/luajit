#include <stdio.h>
#include <inttypes.h>
#include <time.h>
#include <errno.h>
#include "lj_bc.h"


void current_utc_time(struct timespec *ts) {
  int rc;
  rc = clock_gettime(CLOCK_MONOTONIC, ts);
  if (rc != 0) {
    printf("ec: %s\n", strerror(errno));
    exit(errno);
  }
}

typedef struct M {
  char *name;
  double totalTime;
  uint64_t count;
} M;

#define BCM(name, ma, mb, mc, mt)	{ #name, 0, 0 },
M BCOpM[] = {
BCDEF(BCM)
  {"", 0, 0}
};
#undef BCM

int bOp = BC__MAX;
struct timespec tstart;

void lj_measure_start(int op)
{
  struct timespec tend;
  current_utc_time(&tend);
  //printf("OP: %d, Name: %s\n", op, BCOpM[op].name);
  if (bOp != BC__MAX) {
    BCOpM[bOp].totalTime += (((double)tend.tv_sec + 1.0e-9*tend.tv_nsec) - ((double)tstart.tv_sec + 1.0e-9*tstart.tv_nsec));
  }
  bOp = op;
  tstart.tv_sec = tend.tv_sec;
  tstart.tv_nsec = tend.tv_nsec;
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
