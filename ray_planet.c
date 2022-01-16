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

int main(void) {
  InitWindow(screenWidth, screenHeight, "Raylib Planets");
  SetTargetFPS(FPS);
  ToggleFullscreen();

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

  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
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
    }
    if (select) {
      DrawCircleV(mousePos0, 2, MAROON);
      DrawLineV(mousePos0, GetMousePosition(), MAROON);
    } else {
      double t_next = t + t_step;
      gsl_odeiv2_driver_apply(d, &t, t_next, y);
      // printf("sim @ (%.3f + %.3f, %.3f + %.3f);\n", y[0], y[1], y[2], y[3]);
      Vector2 a = {.x = y[0], .y = y[2]};
      Vector2 b = {.x = y[0] + t_step * y[1], .y = y[2] + t_step * y[3]};
      DrawCircleV(sim2scr(a), 4, MAROON);
      // DrawLineV(sim2scr(a), sim2scr(b), MAROON);
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
