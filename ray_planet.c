#include "raylib.h"
#include <math.h>
#include <string.h>

#define RAYGUI_IMPLEMENTATION
#include "extras/raygui.h"
#include <gsl/gsl_errno.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_odeiv2.h>
#include <stdio.h>

typedef struct {
  double x;
  double vx;
  double y;
  double vy;
} VState;

VState VState0 = {0};

typedef struct {
  int n;
  int fill;      // contraint: fill < n
  Vector2 *data; // contraint: points to allocated n-elements
} VTail;

typedef struct {
  VState state;
  Color color;
  VTail *tail;
} Planet;

typedef struct {
  int n;
  Planet **planets;
  VState *state;
  gsl_odeiv2_system *sys;
  gsl_odeiv2_driver *driver;
} PSystem;

// Screen Coordinate System with letters P,Q,..
const int screenWidth = 1600;
const int screenHeight = 900;
const float screenScale = 200.0f;

const int FPS = 60;
const float STEP = 5.0f / (float)FPS;

char FPS_STR[100];

int GRAVITY = 100;
int INTERACTION = 10;

int func(double t, const double y[], double f[], void *params) {
  (void)(t); /* avoid unused parameter warning */
  PSystem *ps = (PSystem *)params;

  memset(f, 0, sizeof(double) * ps->n * 4);

  // INIT
  for (int i = 0; i < ps->n; i++) {
    f[4 * i + 0] = y[4 * i + 1];
    f[4 * i + 2] = y[4 * i + 3];
  }

  // Gravity towards center
  float M = 0.1 * (float)GRAVITY / 100.0f;
  for (int i = 0; i < ps->n; i++) {
    double r3 = pow(pow(y[4 * i + 0], 2) + pow(y[4 * i + 2], 2), 3.f / 2.f);
    if (r3 > 1e-6) {
      f[4 * i + 1] += y[4 * i + 0] / r3 * (-1) * M;
      f[4 * i + 3] += y[4 * i + 2] / r3 * (-1) * M;
    }
  }

  // Interactions
  float C = 0.01 * (float)INTERACTION / 100.0f;
  if (C > 0) {
    for (int i = 0; i < ps->n; i++) {
      for (int j = 0; j < i; j++) {
        if (i != j) {
          double dx = y[4 * j + 0] - y[4 * i + 0]; // from i -> j
          double dy = y[4 * j + 2] - y[4 * i + 2];
          double r3 = pow(pow(dx, 2) + pow(dy, 2), 3.f / 2.f);
          if (r3 > 1e-6) {
            f[4 * i + 1] += C * dx / r3;
            f[4 * i + 3] += C * dy / r3;
            f[4 * j + 1] += -C * dx / r3;
            f[4 * j + 3] += -C * dy / r3;
          }
        }
      }
    }
  }
  return GSL_SUCCESS;
}

//
// Vector Helpers
//
Vector2 V(float x, float y) {
  Vector2 out;
  out.x = x;
  out.y = y;
  return out;
}

Vector2 Vsum(Vector2 a, Vector2 b) { return V(a.x + b.x, a.y + b.y); }

Vector2 Vdiff(Vector2 a, Vector2 b) { return V(a.x - b.x, a.y - b.y); }

Vector2 Vscale(Vector2 a, float s) { return V(a.x * s, a.y * s); }

// Simulation coordinates around 0. Letter a,b
Vector2 scr2sim(Vector2 P) {
  return V((P.x - (float)screenWidth / 2) / screenScale,
           -(P.y - (float)screenHeight / 2) / screenScale);
}

Vector2 sim2scr(Vector2 a) {
  return V((float)screenWidth / 2 + a.x * screenScale,
           (float)screenHeight / 2 - a.y * screenScale);
}
//
// VTail
//

void VTail_print(VTail *t) {
  printf("VTail<n=%d,fill=%d,data=%p>", t->n, t->fill, t->data);
}

VTail *VTail_alloc(int n) {
  VTail *t = calloc(1, sizeof(VTail));
  t->n = n;
  t->fill = 0;
  t->data = calloc(n, sizeof(Vector2));
  return t;
}

void VTail_clear(VTail *t) { t->fill = 0; }

void VTail_push(VTail *t, Vector2 a) {
  if (t->n == 0) {
    return;
  }
  // advance elements in t.data
  for (int tar = t->fill; tar > 0; tar--) {
    int src = tar - 1;
    if (src >= 0 && tar < t->n) {
      t->data[tar] = t->data[src];
    }
  }
  if (t->fill < t->n) {
    t->fill += 1;
  }
  t->data[0] = a;
}

//
// Planet
//

Planet *Planet_alloc(Vector2 pos, Vector2 vel) {
  Planet *p = calloc(1, sizeof(Planet));
  p->state.x = pos.x;
  p->state.y = pos.y;
  p->state.vx = vel.x;
  p->state.vy = vel.y;
  p->tail = VTail_alloc(50);
  p->color = MAROON;
  return p;
}

void Planet_print(Planet *p) {
  printf("Planet< %.3f + %.3f ; %.3f + %.3f >\n", p->state.x, p->state.vx,
         p->state.y, p->state.vy);
}

Vector2 Planet_pos(Planet *p) { return V(p->state.x, p->state.y); }

Vector2 Planet_vel(Planet *p) { return V(p->state.vx, p->state.vy); }

void Planet_reflect(Planet *p) {
  if (sim2scr(Planet_pos(p)).x < 0) {
    p->state.vx = -p->state.vx;
  }
  if (sim2scr(Planet_pos(p)).y < 0) {
    p->state.vy = -p->state.vy;
  }
  if (sim2scr(Planet_pos(p)).x > screenWidth) {
    p->state.vx = -p->state.vx;
  }
  if (sim2scr(Planet_pos(p)).y > screenHeight) {
    p->state.vy = -p->state.vy;
  }
}

double scr_mod(double x, double sz) {
  sz = fabs(sz); // sz may be negative
  double y = fmod(x + sz, 2 * sz) - sz;
  // fix "quadrant"
  while (y < -sz)
    y += 2 * sz;
  while (y > +sz)
    y -= 2 * sz;
  return y;
}

void Planet_move(Planet *p) {
  Vector2 scr = scr2sim(V(screenWidth, screenHeight));
  p->state.x = scr_mod(p->state.x, scr.x);
  p->state.y = scr_mod(p->state.y, scr.y);
}

void Planet_draw(Planet *p) {
  float h = 1.0f / 5.0f;
  DrawCircleV(sim2scr(Planet_pos(p)), 4, MAROON);

  // DrawLineV(sim2scr(Planet_pos(p)), sim2scr(Vsum(Planet_pos(p),
  // Vscale(Planet_vel(p), h))), BLUE);
  VTail_push(p->tail, Planet_pos(p));
  for (int i = 0; i < p->tail->fill; i++) {
    DrawCircleV(sim2scr(p->tail->data[i]), 1, MAROON);
  }
}

void Planet_set(Planet *p, Vector2 pos, Vector2 vel) {
  p->state.x = pos.x;
  p->state.y = pos.y;
  p->state.vx = vel.x;
  p->state.vy = vel.y;
  VTail_clear(p->tail);
}

//
// Planet System
//

PSystem *PSystem_alloc() {
  PSystem *ps = calloc(1, sizeof(PSystem));
  return ps;
}

void PSystem_print(PSystem *ps) {
  printf("PSystem<n=%d,state=[", ps->n);
  for (int i = 0; i < ps->n; i++) {
    printf("%.3f,", ps->state[i].x);
    printf("%.3f,", ps->state[i].vx);
    printf("%.3f,", ps->state[i].y);
    printf("%.3f; ", ps->state[i].vy);
  }
  printf("]>\n");
}

void PSystem_add(PSystem *ps, Planet *p) {
  ps->n += 1;
  ps->planets = realloc(ps->planets, sizeof(Planet *) * ps->n);
  ps->planets[ps->n - 1] = p;

  ps->sys = calloc(1, sizeof(gsl_odeiv2_system));
  ps->sys->function = func;
  ps->sys->jacobian = NULL;
  ps->sys->dimension = ps->n * 4;
  ps->sys->params = ps;
  ps->driver = gsl_odeiv2_driver_alloc_y_new(ps->sys, gsl_odeiv2_step_rk4, 1e-5,
                                             1e-5, 0.0);

  ps->state = realloc(ps->state, sizeof(VState) * ps->n);
  ps->state[ps->n - 1] = VState0;
}

void PSystem_step(PSystem *ps) {
  if (!ps->n) {
    return;
  }
  for (int i = 0; i < ps->n; i++) {
    Planet *p = ps->planets[i];
    ps->state[i] = p->state;
  }

  double t = 0;
  int o = gsl_odeiv2_driver_apply(ps->driver, &t, STEP, (double *)ps->state);
  if (o != GSL_SUCCESS) {
    printf("Simulation error at t=%.3f\n", t);
    exit(1);
  }

  for (int i = 0; i < ps->n; i++) {
    Planet *p = ps->planets[i];
    p->state = ps->state[i];
    // Planet_reflect(p);
    Planet_move(p);
  }
}

void PSystem_draw(PSystem *ps) {
  for (int i = 0; i < ps->n; i++) {
    Planet_draw(ps->planets[i]);
  }
}

float randf(float a) { return 2 * a * (float)rand() / (float)RAND_MAX - a; }

int main(void) {
  InitWindow(screenWidth, screenHeight, "Raylib Planets");
  SetTargetFPS(FPS);

  // Simulation State
  PSystem *ps = PSystem_alloc();

  // UI state
  int select = 0;
  Vector2 mousePos0;

  for (int i = 0; i < 0; i++) {
    Vector2 a = V(randf(2), randf(2));
    Vector2 v = V(randf(.05), randf(.05));
    PSystem_add(ps, Planet_alloc(a, v));
  }

  while (!WindowShouldClose()) // Detect window close button or ESC key
  {

    BeginDrawing();
    ClearBackground(RAYWHITE);

    INTERACTION = GuiSliderBar((Rectangle){600, 40, 120, 20}, "Interaction",
                               NULL, INTERACTION, -450, 450);
    GRAVITY = GuiSliderBar((Rectangle){600, 70, 120, 20}, "EndAngle", NULL,
                           GRAVITY, -450, 450);

    if (IsKeyReleased(KEY_Q)) {
      CloseWindow();
    }
    if (IsKeyReleased(KEY_F)) {
      ToggleFullscreen();
    }
    if (IsKeyReleased(KEY_R)) {
      ps = PSystem_alloc();
    }
    if (IsKeyReleased(KEY_S)) {
      Vector2 a = V(randf(2), randf(2));
      Vector2 v = V(randf(.05), randf(.05));
      PSystem_add(ps, Planet_alloc(a, v));
    }
    if (!select && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      mousePos0 = GetMousePosition();
      select = 1;
      Vector2 a = scr2sim(mousePos0);
    } else if (select && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      Vector2 a = scr2sim(mousePos0);
      Vector2 b = scr2sim(GetMousePosition());
      PSystem_add(ps, Planet_alloc(a, Vdiff(b, a)));
      select = 0;
    }
    if (select) {
      DrawCircleV(mousePos0, 2, MAROON);
      DrawLineV(mousePos0, GetMousePosition(), MAROON);
    }
    PSystem_step(ps);
    PSystem_draw(ps);

    // Center
    DrawCircleV(sim2scr(V(0, 0)), 5, BLACK);
    DrawFPS(0, 0);
    EndDrawing();
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}
