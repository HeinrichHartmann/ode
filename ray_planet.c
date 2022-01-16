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

// Vector helpwers
Vector2 Vadd(Vector2 a, Vector2 b) {
  Vector2 out;
  out.x = a.x + b.x;
  out.y = a.y + b.y;
  return out;
}

Vector2 Vscale(Vector2 a, float s) {
  Vector2 out;
  out.x = a.x * s;
  out.y = a.y * s;
  return out;
}

Vector2 Vdiff(Vector2 a, Vector2 b) { return Vadd(a, Vscale(b, -1)); }

// Simulation coordinates around 0. Letter a,b
Vector2 scr2sim(Vector2 P) {
  Vector2 out;
  out.x = (P.x - (float)screenWidth / 2) / screenScale;
  out.y = -(P.y - (float)screenHeight / 2) / screenScale;
  return out;
}

Vector2 sim2scr(Vector2 a) {
  Vector2 out;
  out.x = (float)screenWidth / 2 + a.x * screenScale;
  out.y = (float)screenHeight / 2 - a.y * screenScale;
  return out;
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
  VTail tail;
  Color color;
} Planet;

Planet *Planet_alloc(Color color, Vector2 pos, Vector2 vel) {
  Planet *out = calloc(1, sizeof(Planet));
  out->color = color;
  out->y[0] = pos.x;
  out->y[1] = vel.x;
  out->y[2] = pos.y;
  out->y[3] = vel.y;
  return out;
}

int main(void) {
  InitWindow(screenWidth, screenHeight, "Raylib Planets");
  SetTargetFPS(FPS);

  int params = 10;
  gsl_odeiv2_system sys = {func, NULL, 4, &params};
  gsl_odeiv2_driver *d =
      gsl_odeiv2_driver_alloc_y_new(&sys, gsl_odeiv2_step_rk4, 1e-6, 1e-6, 0.0);

  // Initial Values
  double t = 0.0;
  double y[4] = {1.0, 0.0, 0.0, 1.0};

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
      printf("Selected (%.3f, %.3f)\n", a.x, a.y);
    } else if (select && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      Vector2 a = scr2sim(mousePos0);
      Vector2 b = scr2sim(GetMousePosition());
      Vector2 d = Vdiff(b, a);
      y[0] = a.x;
      y[1] = d.x;
      y[2] = a.y;
      y[3] = d.y;
      printf("set to (%.3f + %.3f, %.3f + %.3f);\n", y[0], y[1], y[2], y[3]);
      select = 0;
      VTail_clear(&tail);
    }
    if (select) {
      DrawCircleV(mousePos0, 2, MAROON);
      DrawLineV(mousePos0, GetMousePosition(), MAROON);
    } else {
      double t_next = t + t_step;
      gsl_odeiv2_driver_apply(d, &t, t_next, y);
      // printf("sim @ (%.3f + %.3f, %.3f + %.3f);\n", y[0], y[1], y[2], y[3]);
      Vector2 a = {.x = y[0], .y = y[2]};
      DrawCircleV(sim2scr(a), 4, MAROON);

      // Draw velocity
      float h = 5; // v points h frames in advance
      Vector2 b = {.x = y[0] + h * t_step * y[1],
                   .y = y[2] + h * t_step * y[3]};
      DrawLineV(sim2scr(a), sim2scr(b), BLUE);

      // Draw tail
      if (frame_cnt % tail_skip == 1) {
        VTail_push(&tail, sim2scr(a));
      }
      for (int i = 0; i < tail.fill; i++) {
        DrawCircleV(tail.data[i], 1, MAROON);
      }
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
