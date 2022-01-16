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

int FPS = 100;

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
  gsl_odeiv2_driver *driver;
} Planet;

Planet *Planet_alloc(Vector2 pos, Vector2 vel, int tail_len, Color color,
                     gsl_odeiv2_driver *driver) {
  Planet *out = calloc(1, sizeof(Planet));
  out->y[0] = pos.x;
  out->y[1] = vel.x;
  out->y[2] = pos.y;
  out->y[3] = vel.y;
  out->color = color;
  out->tail = VTail_alloc(tail_len);
  out->driver = driver;
  return out;
}

void Planet_step(Planet *p, float t_step) {
  double t = 0;
  gsl_odeiv2_driver_apply(p->driver, &t, t_step, p->y);
}

Vector2 Planet_pos(Planet *p) { return V(p->y[0], p->y[2]); }

Vector2 Planet_vel(Planet *p) { return V(p->y[1], p->y[3]); }

void Planet_draw(Planet *p) {
  float h = 1.0f / 5.0f;
  DrawCircleV(sim2scr(Planet_pos(p)), 4, MAROON);
  DrawLineV(sim2scr(Planet_pos(p)),
            sim2scr(Vsum(Planet_pos(p), Vscale(Planet_vel(p), h))), BLUE);
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

int main(void) {
  InitWindow(screenWidth, screenHeight, "Raylib Planets");
  SetTargetFPS(FPS);

  int params = 10;
  gsl_odeiv2_system sys = {func, NULL, 4, &params};
  gsl_odeiv2_driver *d =
      gsl_odeiv2_driver_alloc_y_new(&sys, gsl_odeiv2_step_rk4, 1e-6, 1e-6, 0.0);

  Planet *p = Planet_alloc(V(1, 0), V(0, 1), 20, MAROON, d);

  // UI state
  int select = 0;
  Vector2 ballPosition = {0};
  Vector2 mousePos0;
  float t_step = 2.0f / (float)FPS;
  int tail_len = 100;
  int tail_skip = 2;
  VTail tail = VTail_alloc(tail_len);
  int frame_cnt = 0;

  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    frame_cnt += 1;
    BeginDrawing();
    ClearBackground(RAYWHITE);

    if (!select && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      mousePos0 = GetMousePosition();
      select = 1;
      Vector2 a = scr2sim(mousePos0);
    } else if (select && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      Vector2 a = scr2sim(mousePos0);
      Vector2 b = scr2sim(GetMousePosition());
      Planet_set(p, a, Vdiff(b, a));
      select = 0;
    }
    if (select) {
      DrawCircleV(mousePos0, 2, MAROON);
      DrawLineV(mousePos0, GetMousePosition(), MAROON);
    } else {
      Planet_step(p, t_step);
      Planet_draw(p);
    }
    Vector2 orig = {0};
    DrawCircleV(sim2scr(orig), 5, BLACK);
    EndDrawing();
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}
