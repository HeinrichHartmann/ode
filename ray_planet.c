#include "raylib.h"
#include <string.h>

#include <gsl/gsl_errno.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_odeiv2.h>
#include <stdio.h>

int func(double t, const double y[], double f[], void *params) {
  (void)(t); /* avoid unused parameter warning */
  (void)(params);
  f[0] = y[1];  // x
  f[1] = -y[0]; // x'
  f[2] = y[3];  // y
  f[3] = -y[2]; // y'
  return GSL_SUCCESS;
}

// Screen Coordinate System with letters P,Q,..
const int screenWidth = 800;
const int screenHeight = 600;
const float screenScale = 200.0f;

const int FPS = 100;
const float STEP = 2.0f / (float)FPS;

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

typedef struct {
  int n;
  int fill;      // contraint: fill < n
  Vector2 *data; // contraint: points to allocated n-elements
} VTail;

void VTail_print(VTail *tp) {
  printf("VTail<n=%d,fill=%d,data=%p>", tp->n, tp->fill, tp->data);
}

VTail VTail_alloc(int n) {
  VTail out;
  out.n = n;
  out.fill = 0;
  out.data = calloc(n, sizeof(Vector2));
  return out;
}

void VTail_clear(VTail *t) { t->fill = 0; }

void VTail_push(VTail *t, Vector2 a) {
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

typedef struct {
  double y[4];
  double t;
  VTail tail;
  Color color;
} Planet;

Planet *Planet_alloc(Vector2 pos, Vector2 vel, int tail_len, Color color) {
  Planet *out = calloc(1, sizeof(Planet));
  out->y[0] = pos.x;
  out->y[1] = vel.x;
  out->y[2] = pos.y;
  out->y[3] = vel.y;
  out->color = color;
  out->tail = VTail_alloc(tail_len);
  return out;
}

void Planet_step(Planet *p, gsl_odeiv2_driver *d) {
  double t = 0;
  gsl_odeiv2_driver_apply(d, &t, STEP, p->y);
}

Vector2 Planet_pos(Planet *p) { return V(p->y[0], p->y[2]); }

Vector2 Planet_vel(Planet *p) { return V(p->y[1], p->y[3]); }

void Planet_draw(Planet *p) {
  float h = 1.0f / 5.0f;
  DrawCircleV(sim2scr(Planet_pos(p)), 4, MAROON);
  // DrawLineV(sim2scr(Planet_pos(p)), sim2scr(Vsum(Planet_pos(p),
  // Vscale(Planet_vel(p), h))), BLUE);
  VTail_push(&p->tail, Planet_pos(p));
  for (int i = 0; i < p->tail.fill; i++) {
    DrawCircleV(sim2scr(p->tail.data[i]), 1, MAROON);
  }
}

void Planet_set(Planet *p, Vector2 pos, Vector2 vel) {
  p->y[0] = pos.x;
  p->y[1] = vel.x;
  p->y[2] = pos.y;
  p->y[3] = vel.y;
  VTail_clear(&p->tail);
}

typedef struct {
  int n;
  Planet **planets;
  float center_mass;
  gsl_odeiv2_system *sys;
  gsl_odeiv2_driver *driver;
} PSystem;

PSystem *PSystem_alloc(float center_mass) {
  PSystem *ps = calloc(1, sizeof(PSystem));
  ps->sys = calloc(1, sizeof(gsl_odeiv2_system));
  ps->sys->function = func;
  ps->sys->jacobian = NULL;
  ps->sys->dimension = 4;
  ps->sys->params = ps;
  ps->driver = gsl_odeiv2_driver_alloc_y_new(ps->sys, gsl_odeiv2_step_rk4, 1e-6,
                                             1e-6, 0.0);
  return ps;
}

void PSystem_add(PSystem *ps, Planet *p) {
  ps->n += 1;
  ps->planets = realloc(ps->planets, sizeof(Planet *) * ps->n);
  ps->planets[ps->n - 1] = p;
}

void PSystem_step(PSystem *ps) {
  for (int i = 0; i < ps->n; i++) {
    Planet_step(ps->planets[i], ps->driver);
  }
}

void PSystem_draw(PSystem *ps) {
  for (int i = 0; i < ps->n; i++) {
    Planet_draw(ps->planets[i]);
  }
}

int main(void) {
  InitWindow(screenWidth, screenHeight, "Raylib Planets");
  SetTargetFPS(FPS);

  // Simulation State
  PSystem *ps = PSystem_alloc(1);

  // UI state
  int select = 0;
  Vector2 mousePos0;

  while (!WindowShouldClose()) // Detect window close button or ESC key
  {

    BeginDrawing();
    ClearBackground(RAYWHITE);
    if (IsKeyReleased(KEY_F)) {
      ToggleFullscreen();
    }
    if (!select && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      mousePos0 = GetMousePosition();
      select = 1;
      Vector2 a = scr2sim(mousePos0);
    } else if (select && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      Vector2 a = scr2sim(mousePos0);
      Vector2 b = scr2sim(GetMousePosition());
      PSystem_add(ps, Planet_alloc(a, Vdiff(b, a), 50, MAROON));
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
    EndDrawing();
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}
