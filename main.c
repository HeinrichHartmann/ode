#include <stdio.h>
#include <stdlib.h>

const float STEP = 1e-3;
const int PSTEP = 10;

typedef double num;
typedef struct {
  num x;
  num v;
} pt; // point type

// ODE function p' = F(t, p)
void F(pt *out, num t, pt *p) {
  // assumes ot is initiatized to 0 already
  out->x = p->v;
  out->v = -p->x;
};

void step(pt *p) {
  pt q = *p; // copy
  pt v = {0};
  F(&v, 0, p);
  p->x = q.x + v.x * STEP;
  p->v = q.v + v.v * STEP;
}

void runge_step(pt *p) {
  pt q = *p;
  pt eta = {0};
  F(&eta, 0, p);
  eta.x = q.x + eta.x * STEP / 2;
  eta.v = q.v + eta.v * STEP / 2;
  pt delta = {0};
  F(&delta, 0, &eta);
  p->x = q.x + delta.x * STEP;
  p->v = q.v + delta.v * STEP;
}

void heun_step(pt *p) {
  pt q = *p;
  pt eta = {0};
  F(&eta, 0, p);
  eta.x = q.x + eta.x * STEP / 2;
  eta.v = q.v + eta.v * STEP / 2;
  pt delta = {0};
  F(&delta, 0, &eta);
  p->x = eta.x + delta.x * STEP / 2;
  p->v = eta.v + delta.v * STEP / 2;
}

int main(int argc, char **argv) {
  long stop = -1;
  if (argc > 1) {
    stop = strtol(*(argv + 1), (char **)NULL, 10);
  }
  pt state[1] = {{.0, -5}};
  int s = 0;
  while (s / PSTEP != stop) {
    heun_step(state);
    if (s++ % PSTEP == 0) {
      printf("%.6f\n", state[0].x);
    }
  }
  return 0;
}
