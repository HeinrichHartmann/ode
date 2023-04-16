#include <math.h>
#include <stdio.h>
#include <string.h>

#include <gsl/gsl_errno.h>
#include <gsl/gsl_matrix.h>
#include <gsl/gsl_odeiv2.h>
#include <gsl/gsl_randist.h>
#include <gsl/gsl_rng.h>

#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h" // Required for GUI controls

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
int screenWidth = 1600;
int screenHeight = 900;
gsl_rng *rng;

const int FPS = 60;
const float STEP = 5.0f / (float)FPS;

int GRAVITY = 0; // size of sun
int INTERACTION = 0;
int SCALE = 0;
int TOPOLOGY = 0; // 0: rectangle / 1: torus

// Evaluate function at time t, state y and store result in dydt
int func(double t, const double y[], double dydt[], void *params) {
  (void)(t); /* avoid unused parameter warning */
  PSystem *ps = (PSystem *)params;

  memset(dydt, 0, sizeof(double) * ps->n * 4);

  // INIT
  for (int i = 0; i < ps->n; i++) {
    dydt[4 * i + 0] = y[4 * i + 1];
    dydt[4 * i + 2] = y[4 * i + 3];
  }

  // Gravity towards center
  float M = (float)GRAVITY / 100.0f;
  for (int i = 0; i < ps->n; i++) {
    double r3 = pow(pow(y[4 * i + 0], 2) + pow(y[4 * i + 2], 2), 3.f / 2.f);
    if (r3 > 1e-6) {
      dydt[4 * i + 1] += y[4 * i + 0] / r3 * (-1) * M;
      dydt[4 * i + 3] += y[4 * i + 2] / r3 * (-1) * M;
    }
  }

  // Interactions
  float C = 0.01 * INTERACTION;
  if (C != 0) {
    for (int i = 0; i < ps->n; i++) {
      for (int j = 0; j < i; j++) {
        if (i != j) {
          double dx = y[4 * j + 0] - y[4 * i + 0]; // from i -> j
          double dy = y[4 * j + 2] - y[4 * i + 2];
          double r3 = pow(pow(dx, 2) + pow(dy, 2), 3.f / 2.f);
          if (r3 > 1e-6) {
            dydt[4 * i + 1] += C * dx / r3;
            dydt[4 * i + 3] += C * dy / r3;
            dydt[4 * j + 1] += -C * dx / r3;
            dydt[4 * j + 3] += -C * dy / r3;
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
  float s = (float)SCALE * 20.f + 200;
  return V((P.x - (float)screenWidth / 2) / s,
           -(P.y - (float)screenHeight / 2) / s);
}

Vector2 sim2scr(Vector2 a) {
  float s = (float)SCALE * 20.f + 200;
  return V((float)screenWidth / 2 + a.x * s, (float)screenHeight / 2 - a.y * s);
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
  if (sim2scr(Planet_pos(p)).x < 10) {
    p->state.vx = -p->state.vx;
  }
  if (sim2scr(Planet_pos(p)).y < 10) {
    p->state.vy = -p->state.vy;
  }
  if (sim2scr(Planet_pos(p)).x > screenWidth - 10) {
    p->state.vx = -p->state.vx;
  }
  if (sim2scr(Planet_pos(p)).y > screenHeight - 10) {
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

void Planet_draw(Planet *p) {
  Color c = BLUE;
  if (INTERACTION < 0) {
    c = MAROON;
  }
  float sz = 0.3f * abs(INTERACTION);
  DrawCircleV(sim2scr(Planet_pos(p)), sz, c);

  VTail_push(p->tail, Planet_pos(p));
  int n = p->tail->fill;
  for (int i = 0; i < n; i++) {
    float f = 1 - (float)i / (float)n;
    DrawCircleV(sim2scr(p->tail->data[i]), 1, Fade(c, f));
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
    if (TOPOLOGY == 1) { // Torus
      Vector2 scr = scr2sim(V(screenWidth, screenHeight));
      p->state.x = scr_mod(p->state.x, scr.x);
      p->state.y = scr_mod(p->state.y, scr.y);
    } else { // Reflecting Rectangle
      Planet_reflect(p);
    }
  }
}

void PSystem_draw(PSystem *ps) {
  for (int i = 0; i < ps->n; i++) {
    Planet_draw(ps->planets[i]);
  }
}

void PSystem_freeze(PSystem *ps, float s) {
  for (int i = 0; i < ps->n; i++) {
    ps->planets[i]->state.vx *= s;
    ps->planets[i]->state.vy *= s;
  }
}

float PSystem_energy(PSystem *ps) {
  float e = 0;
  for (int i = 0; i < ps->n; i++) {
    Planet *p = ps->planets[i];
    e += 0.5 * pow(p->state.vx, 2), pow(p->state.vy, 2);
  }
  return e;
}

void PSystem_shock(PSystem *ps, float sigma) {
  float e = sqrt(PSystem_energy(ps));
  for (int i = 0; i < ps->n; i++) {
    ps->planets[i]->state.vx += gsl_ran_gaussian(rng, e * sigma);
    ps->planets[i]->state.vy += gsl_ran_gaussian(rng, e * sigma);
  }
}

void PSystem_center(PSystem *ps) {
  float cx = 0, cy = 0, cvx = 0, cvy = 0;
  for (int i = 0; i < ps->n; i++) {
    cx += ps->planets[i]->state.x;
    cy += ps->planets[i]->state.y;
    cvx += ps->planets[i]->state.vx;
    cvy += ps->planets[i]->state.vy;
  }
  cx = cx / ps->n;
  cy = cy / ps->n;
  cvx = cvx / ps->n;
  cvy = cvy / ps->n;
  printf("C %f %f\n", cvx, cvy);
  for (int i = 0; i < ps->n; i++) {
    ps->planets[i]->state.x -= cx;
    ps->planets[i]->state.y -= cy;
    ps->planets[i]->state.vx -= cvx;
    ps->planets[i]->state.vy -= cvy;
  }
}

float randf(float a) { return 2 * a * (float)rand() / (float)RAND_MAX - a; }


void key_ctrl(int *x, int key) {
  if (!IsKeyDown(key)) return;
  if (IsKeyDown(KEY_LEFT_CONTROL)) {
    *x = 0;
    return;
  }
  int d = IsKeyDown(KEY_LEFT_SHIFT) ? -1 : 1;
  *x += d;
  if (*x < -100) {
    *x = -100;
  }
  if (*x > 100) {
    *x = 100;
  }
}

int main(void) {
  // SetConfigFlags(FLAG_WINDOW_RESIZABLE);
  SetConfigFlags(FLAG_VSYNC_HINT);

  InitWindow(0, 0, "Raylib Planets");
  ToggleFullscreen();
  screenWidth = GetScreenWidth();
  screenHeight = GetScreenHeight();
  printf("S SZ %d %d\n", screenWidth, screenHeight);

  // SetTargetFPS(FPS);
  rng = gsl_rng_alloc(gsl_rng_taus);

  // Simulation State
  PSystem *ps = PSystem_alloc();

  // UI state
  int select = 0;
  Vector2 mousePos0;
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    if (IsKeyReleased(KEY_F)) {
      int monitor = GetCurrentMonitor();
      screenHeight = GetMonitorHeight(monitor);
      screenWidth = GetMonitorWidth(monitor);
      SetWindowSize(screenWidth, screenHeight);
      printf("S SZ %d %d\n", screenWidth, screenHeight);
      ToggleFullscreen();
    }

    screenWidth = GetScreenWidth();
    screenHeight = GetScreenHeight();

    BeginDrawing();
    ClearBackground(BLACK);

    if (IsWindowResized()) {
      screenWidth = GetScreenWidth();
      screenHeight = GetScreenHeight();
    }

    // Daraw Margin
    DrawRectangleLines(10, 10, screenWidth - 20, screenHeight - 20, RAYWHITE);

    // Draw Sun
    Color s_color;
    float s_size = 5 * GRAVITY / 10.0f;;
    if (GRAVITY > 0) {
      s_color = YELLOW;
    } else {
      s_color = MAROON;
      s_size *= -1;
    }
    DrawCircleV(sim2scr(V(0, 0)), s_size, s_color);


    key_ctrl(&GRAVITY, KEY_ONE);
    key_ctrl(&INTERACTION, KEY_TWO);
    key_ctrl(&SCALE, KEY_THREE);
    if (IsKeyPressed(KEY_NINE))
      PSystem_shock(ps, 1.05);
    if (IsKeyDown(KEY_ZERO))
      PSystem_freeze(ps, 0.9);

    if (IsKeyReleased(KEY_C)) {
      PSystem_center(ps);
    }

    if (IsKeyReleased(KEY_Q)) {
      CloseWindow();
    }
    if (IsKeyReleased(KEY_R)) {
      ps = PSystem_alloc();
    }
    if (IsKeyPressed(KEY_S)) {
      float sigma = 1.2;
      float v_sigma = 0.2;
      Vector2 a = V(gsl_ran_gaussian(rng, sigma), gsl_ran_gaussian(rng, sigma));
      Vector2 v =
          V(gsl_ran_gaussian(rng, v_sigma), gsl_ran_gaussian(rng, v_sigma));
      PSystem_add(ps, Planet_alloc(a, v));
    }
    if (!select && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
      mousePos0 = GetMousePosition();
      select = 1;
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

    DrawFPS(15, 15);
    DrawText(TextFormat("%2g Energy", PSystem_energy(ps)), 15, 35, 20, GREEN);
    EndDrawing();
  }

  // De-Initialization
  //--------------------------------------------------------------------------------------
  CloseWindow(); // Close window and OpenGL context
  //--------------------------------------------------------------------------------------

  return 0;
}
