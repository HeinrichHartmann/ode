#include "raylib.h"
#include "math.h"

int main(void)
{
    // Initialization
    //--------------------------------------------------------------------------------------
    const int screenWidth = 800;
    const int screenHeight = 450;

    InitWindow(screenWidth, screenHeight, "raylib [core] example - keyboard input");

    Vector2 ballPosition = { (float)screenWidth/2, (float)screenHeight/2 };

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    double t = 0;
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        t += 1e-2;
        ballPosition.x = (float)screenWidth/2;
        ballPosition.y = (float)screenHeight/2 + 200.0f * cos(t);
        BeginDrawing();

            ClearBackground(RAYWHITE);

            DrawCircleV(ballPosition, 3, MAROON);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
