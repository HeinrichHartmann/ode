#include "math.h"
#include "raylib.h"

#include <gsl/gsl_errno.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_odeiv2.h>
#include <stdio.h>

int func(double t, const double y[], double f[], void *params) {
  (void)(t); /* avoid unused parameter warning */
  (void)(params);
  f[0] = y[1];
  f[1] = -y[0] - 0.8 * (y[0] - y[2]);
  f[2] = y[3];
  f[3] = -y[2] - 0.8 * (y[2] - y[0]);
  return GSL_SUCCESS;
}

int main(void) {
  // Initialization
  //--------------------------------------------------------------------------------------
  const int screenWidth = 800;
  const int screenHeight = 450;
  InitWindow(screenWidth, screenHeight,
             "raylib [core] example - keyboard input");
  Vector2 ballPosition = {0};
  Vector2 ballPosition2 = {0};
  SetTargetFPS(60); // Set our game to run at 60 frames-per-second

  double mu = 10;
  gsl_odeiv2_system sys = {func, NULL, 4, &mu};
  gsl_odeiv2_driver *d =
      gsl_odeiv2_driver_alloc_y_new(&sys, gsl_odeiv2_step_rk4, 1e-6, 1e-6, 0.0);
  double t = 0.0;
  double y[4] = {1.0, 0.0, -1.0, 0.0};

  int select = 0;
  Vector2 mousePos0;

  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    if (select && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      Vector2 mousePos1 = GetMousePosition();
      y[0] = ((float)mousePos0.x - (float)screenWidth / 2) / 200.0f;
      y[1] = (float)(mousePos1.x - mousePos0.x) / 200.0f;
      select = 0;
      printf("Selected %f %f\n", y[0], y[1]);
    } else if (!select && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      printf("Select!\n");
      mousePos0 = GetMousePosition();
      select = 1;
    }
    if (select) {
      y[0] = ((float)mousePos0.x - (float)screenWidth / 2) / 200.0f;
      y[1] = 0;
    }

    double t_next = t + 5 * 1e-2;
    gsl_odeiv2_driver_apply(d, &t, t_next, y);
    // printf("%.5e %.5e %.5e\n", t, y[0], y[1]);
    ballPosition.x = (float)screenWidth / 2 + 200.0f * y[0];
    ballPosition.y = (float)screenHeight / 2;
    ballPosition2.x = (float)screenWidth / 2 + 200.0f * y[2];
    ballPosition2.y = (float)screenHeight / 2;

    BeginDrawing();
    ClearBackground(RAYWHITE);
    DrawCircleV(ballPosition, 3, MAROON);
    DrawCircleV(ballPosition2, 3, BLACK);
    EndDrawing();
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}
