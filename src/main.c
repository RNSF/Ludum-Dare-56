#include <stdlib.h>
#include <assert.h>
#include "raylib.h"
#include "game.h"
#include "array.h"
#include "hash_table.h"
#include "raymath.h"

#define ITERATE(IDX, LEN) (size_t IDX = 0; IDX < LEN; IDX++)
#define DEBUG_ENABLED true
#define CHUNK_SIZE (Vector2){16, 16}
#define FPS 60
#define FIXED_DELTA 1.0/FPS
#define BOID_CHUNK_MAX 32

////////////////////////////////////////////
// ..Enum
////////////////////////////////////////////



////////////////////////////////////////////
// ..Header
////////////////////////////////////////////

typedef struct Boid {
    Vector2 position;
    Vector2 velocity;
} Boid;

typedef struct BoidMap {
    Array chunks; //BoidChunk
    Vector2 chunkCount;
} BoidMap;

typedef struct BoidChunk {
    Boid* boids[BOID_CHUNK_MAX];
    int boidCount;
} BoidChunk;

typedef struct Snake {
    Vector2 position;
    Vector2 velocity;
} Snake;

typedef struct World {
    Array boids;
    BoidMap boidMap;
    Snake snake;
    Rectangle bounds;
    Rectangle waterBounds;
} World;


////////////////////////////////////////////
// ..Color
////////////////////////////////////////////



////////////////////////////////////////////
// ..Math
////////////////////////////////////////////

float Vector2Area(Vector2 v1) {
    return v1.x * v1.y;
}

Vector2 Vector2Floor(Vector2 v1) {
    return (Vector2){floor(v1.x), floor(v1.y)};
}

Vector2 Vector2Ceil(Vector2 v1) {
    return (Vector2){ceil(v1.x), ceil(v1.y)};
}

Vector2 Vector2Max(Vector2 v1, Vector2 v2) {
    return (Vector2){fmax(v1.x, v2.x), fmax(v1.y, v2.y)};
}

Vector2 Vector2Min(Vector2 v1, Vector2 v2) {
    return (Vector2){fmin(v1.x, v2.x), fmin(v1.y, v2.y)};
}

Vector2 Vector2Side(float l) {
    return (Vector2){l, l};
}

Rectangle RectangleFromSize(Vector2 size) {
    return (Rectangle) {0, 0, size.x, size.y};
}

Rectangle RectangleCorners(Vector2 topLeft, Vector2 bottomRight) {
    return (Rectangle) {topLeft.x, topLeft.y, bottomRight.x - topLeft.x, bottomRight.y - topLeft.y};
}

Vector2 RectangleSize(Rectangle rect) {
    return (Vector2){rect.width, rect.height};
}

Vector2 RectangleTopLeft(Rectangle rect) {
    return (Vector2){rect.x, rect.y};
}

Vector2 RectangleBottomRight(Rectangle rect) {
    return (Vector2){rect.x + rect.width, rect.y + rect.height};
}

Vector2 RectangleCenter(Rectangle rect) {
    return (Vector2){rect.x + rect.width / 2, rect.y + rect.height / 2};
}

float RectangleLeft(Rectangle rect) {
    return rect.x;
}

float RectangleTop(Rectangle rect) {
    return rect.y;
}

float RectangleRight(Rectangle rect) {
    return rect.x + rect.width;
}

float RectangleBottom(Rectangle rect) {
    return rect.y + rect.height;
}

////////////////////////////////////////////
// ..Images
////////////////////////////////////////////

void loadSprites() {
    
}


////////////////////////////////////////////
// ..Boid
////////////////////////////////////////////



void initBoidMap(BoidMap* boidMap, Vector2 chunkCount) {
    boidMap->chunkCount = chunkCount;
    boidMap->chunks = aCreate(chunkCount.x * chunkCount.y, sizeof(BoidChunk));
    boidMap->chunks.used = boidMap->chunks.size;
}

void cleanBoidMap(BoidMap* boidMap) {
    aFree(&boidMap->chunks);
}


Vector2 getChunkPosition(Vector2 position) {
    return Vector2Floor(Vector2Divide(position, CHUNK_SIZE));
}

Vector2 getTopLeftOfChunk(Vector2 chunkPosition) {
    return Vector2Multiply(chunkPosition, CHUNK_SIZE);
}

Vector2 getCenterOfChunk(Vector2 chunkPosition) {
    return Vector2Multiply(Vector2Add(chunkPosition, (Vector2){0.5, 0.5}), CHUNK_SIZE);
}


BoidChunk* getChunk(BoidMap* boidMap, Vector2 chunkPosition) {
    assert(chunkPosition.x == round(chunkPosition.x) && chunkPosition.y == round(chunkPosition.y));

    bool hasChunk = CheckCollisionPointRec(chunkPosition, RectangleFromSize(boidMap->chunkCount));
    if (!hasChunk)
        return NULL;

    unsigned int chunkId = chunkPosition.x + chunkPosition.y * boidMap->chunkCount.x;
    BoidChunk* boidChunk = aGet(&boidMap->chunks, chunkId);
    return boidChunk;
}

void getBoidsIn(BoidMap* boidMap, Vector2 position, float radius, Array* output) {
    assert(output->elementSize == sizeof(Boid*));

    Vector2 minChunk = Vector2Max(Vector2Zero(),        Vector2Floor(Vector2Divide(Vector2Add(position, Vector2Side(-radius)), CHUNK_SIZE)));
    Vector2 maxChunk = Vector2Min(boidMap->chunkCount,  Vector2Ceil (Vector2Divide(Vector2Add(position, Vector2Side(+radius)), CHUNK_SIZE)));
    
    Vector2 chunkPosition;
    for (chunkPosition.y = minChunk.y; chunkPosition.y < maxChunk.y; chunkPosition.y++)
    for (chunkPosition.x = minChunk.x; chunkPosition.x < maxChunk.x; chunkPosition.x++) {
        BoidChunk* chunk = getChunk(boidMap, chunkPosition);
        assert(chunk);
        
        Vector2 displacement = Vector2Subtract(getCenterOfChunk(chunkPosition), position);
       
        Vector2 closeSide = (Vector2){displacement.x < 0, displacement.y < 0};
        Vector2 closestPositionInChunk = getTopLeftOfChunk(Vector2Add(chunkPosition, closeSide));
        if (Vector2Distance(closestPositionInChunk, position) > radius) {
            // can just ignore the whole chunk without checking distances of each boid
            continue;
        }

        Vector2 farSide = (Vector2){displacement.x >= 0, displacement.y >= 0};
        Vector2 furthestPositionInChunk = getTopLeftOfChunk(Vector2Add(chunkPosition, farSide));
        if (Vector2Distance(furthestPositionInChunk, position) <= radius && chunk->boidCount) {
            // can just append the whole chunk without checking distances of each boid
            aAppendStaticArray(output, chunk->boids, chunk->boidCount); 
            continue;
        }

        // append boid only if within radius
        for ITERATE(i, chunk->boidCount) {
            Boid* boid = chunk->boids[i];
            if (Vector2Distance(boid->position, position) <= radius) {
                aAppend(output, &boid);
            }
        }
    }
}



#define BOID_VISION_RADIUS 40
#define BOID_SEPARATION_RADIUS 16
#define BOID_AVOIDANCE_RADIUS 100
#define BOID_WALL_VISION_RADIUS 100

#define BOID_SEPARATION_C 50
#define BOID_ALIGNMENT_C 50
#define BOID_COHESION_C 0.5
#define BOID_WALL_C 2000.0
#define BOID_AVOIDANCE_C 8000.0

#define BOID_MIN_SPEED 100
#define BOID_MAX_SPEED 200


void clearBoidMap(BoidMap* boidMap) {
    for ITERATE(i, boidMap->chunks.used) {
        BoidChunk* boidChunk = aGet(&boidMap->chunks, i);
        boidChunk->boidCount = 0;
    }
}



void populateBoidMap(BoidMap* boidMap, Array* boids) {
    for ITERATE(i, boids->used) {
        Boid* boid = aGet(boids, i);
        BoidChunk* boidChunk = getChunk(boidMap, getChunkPosition(boid->position));
        
        if (!boidChunk) {
           //  printf("Boid outside of world!");
            return;
        }

        if (boidChunk->boidCount >= BOID_CHUNK_MAX) {
           //  printf("Boid chunk overflow!");;
            return;
        }

        boidChunk->boids[boidChunk->boidCount] = boid;
        boidChunk->boidCount++;
    }
}

void spawnBoid(World* world, Vector2 position) {
    Boid boid = {position, (Vector2){BOID_MAX_SPEED, 0}};
    aAppend(&world->boids, &boid);
    //printf("SPAWN BOID\n");
}

void boidsReact(Array* boids, BoidMap* boidMap, Rectangle bounds, Vector2 pointToAvoid, float delta) {

    Array flockBoids = aCreate(128, sizeof(Boid*));
    for ITERATE(i, boids->used) {
        flockBoids.used = 0;
        Boid* boid = aGet(boids, i);
        getBoidsIn(boidMap, boid->position, BOID_VISION_RADIUS, &flockBoids);

        Vector2 separationForce = Vector2Zero();
        Vector2 alignmentForce  = Vector2Zero();
        Vector2 cohesionForce   = Vector2Zero();
        Vector2 avoidanceForce  = Vector2Zero();
        Vector2 wallForce   = Vector2Zero();

        if (flockBoids.used) {
            Vector2 totalDisplacement = Vector2Zero();
            Vector2 totalVelocity = Vector2Zero();
            Vector2 totalPosition = Vector2Zero();

            for ITERATE(j, flockBoids.used) {
                Boid* flockBoid = *(Boid**) aGet(&flockBoids, j);
                //if (flockBoid == boid) continue;

                Vector2 displacement = Vector2Subtract(boid->position, flockBoid->position);
                if (Vector2LengthSqr(displacement) <= BOID_SEPARATION_RADIUS * BOID_SEPARATION_RADIUS) 
                    totalDisplacement = Vector2Add(totalDisplacement, displacement); // separation: s BOID_AVOIDANCE_RADIUS 40teer to avoid crowding local flockmates
                totalVelocity = Vector2Add(totalVelocity, flockBoid->velocity);      // alignment: steer towards the average heading of local flockmates
                totalPosition = Vector2Add(totalPosition, flockBoid->position);      // cohesion: steer to move towards the average position (center of mass) of local flockmates
            }

            separationForce = Vector2Scale(totalDisplacement, BOID_SEPARATION_C);
            alignmentForce  = Vector2Scale(totalVelocity  , BOID_ALIGNMENT_C / flockBoids.used);
            cohesionForce   = Vector2Scale(Vector2Subtract(Vector2Scale(totalPosition, 1.0 / flockBoids.used), boid->position), BOID_COHESION_C);
        }
        
        
        if (fabs(boid->position.x - RectangleLeft(bounds)) < BOID_WALL_VISION_RADIUS)   wallForce.x += 1;
        if (fabs(boid->position.x - RectangleRight(bounds)) < BOID_WALL_VISION_RADIUS)  wallForce.x -= 1;
        if (fabs(boid->position.y - RectangleTop(bounds)) < BOID_WALL_VISION_RADIUS)    wallForce.y += 1;
        if (fabs(boid->position.y - RectangleBottom(bounds)) < BOID_WALL_VISION_RADIUS) wallForce.y -= 1;

        wallForce = Vector2Scale(wallForce, BOID_WALL_C);

        Vector2 avoidanceDisplacement = Vector2Subtract(boid->position, pointToAvoid);
        float avoidanceDisplacementLengthSqr = Vector2LengthSqr(avoidanceDisplacement);
        if (avoidanceDisplacementLengthSqr < BOID_AVOIDANCE_RADIUS * BOID_AVOIDANCE_RADIUS)
            avoidanceForce = Vector2Scale(Vector2Normalize(avoidanceDisplacement), BOID_AVOIDANCE_C);

        Vector2 force = Vector2Add(Vector2Add(Vector2Add(Vector2Add(separationForce, alignmentForce), cohesionForce), wallForce), avoidanceForce);

        if (Vector2Length(force)) {
            boid->velocity = Vector2Add(boid->velocity, Vector2Scale(force, delta));
            boid->velocity = Vector2Scale(Vector2Normalize(boid->velocity), Clamp(Vector2Length(boid->velocity), BOID_MIN_SPEED, BOID_MAX_SPEED));
        }
    }

    aFree(&flockBoids);
}

void boidsMove(Array* boids, Rectangle bounds, float delta) {
    for ITERATE(i, boids->used) {
         Boid* boid = aGet(boids, i);
         boid->position = Vector2Add(boid->position, Vector2Scale(boid->velocity, delta));
         boid->position = Vector2Clamp(boid->position, RectangleTopLeft(bounds), Vector2Subtract(RectangleBottomRight(bounds), Vector2Side(0.001)));
    }
}

void boidsRender(Array* boids)
{
    for ITERATE(i, boids->used) {
        Boid* boid = aGet(boids, i);
        DrawCircle(boid->position.x, boid->position.y, 2, WHITE);
    }
}


////////////////////////////////////////////
// ..Snake
////////////////////////////////////////////

#define SNAKE_MAX_SPEED 300
#define SNAKE_ACCELERATION 600


void snakeUpdate(Snake* snake, float delta) {
    Vector2 targetDirection = Vector2Normalize((Vector2) {
        IsKeyDown(KEY_D) - IsKeyDown(KEY_A),
        IsKeyDown(KEY_S) - IsKeyDown(KEY_W),
    });

    Vector2 targetVelocity = Vector2Scale(targetDirection, SNAKE_MAX_SPEED);
    Vector2 velocityDifference = Vector2Subtract(targetVelocity, snake->velocity);
    Vector2 accelerationDirection = Vector2Normalize(velocityDifference);
    snake->velocity = Vector2Add(snake->velocity, Vector2ClampValue(Vector2Scale(accelerationDirection, SNAKE_ACCELERATION * delta), 0, Vector2Length(velocityDifference)));
    snake->velocity = Vector2ClampValue(snake->velocity, 0, SNAKE_MAX_SPEED);
}

void snakeMove(Snake* snake, float delta) {
    snake->position = Vector2Add(snake->position, Vector2Scale(snake->velocity, delta));
}

void snakeRender(Snake* snake) {
    DrawCircle(snake->position.x, snake->position.y, 14, WHITE);
}



////////////////////////////////////////////
// ..World
////////////////////////////////////////////

void initWorld(World* world, Rectangle bounds, float waterLine)
{
    world->boids = aCreate(128, sizeof(Boid));
    initBoidMap(&world->boidMap, Vector2Ceil(Vector2Divide(RectangleSize(bounds), CHUNK_SIZE)));
    world->snake.position = RectangleCenter(bounds);
    world->bounds = bounds;
    world->waterBounds = bounds;
    world->waterBounds.y += waterLine;
    world->waterBounds.height -= waterLine;
}

void cleanWorld(World* world)
{
    aFree(&world->boids);
    cleanBoidMap(&world->boidMap);
}

////////////////////////////////////////////
// ..Main
////////////////////////////////////////////



//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void) {
    // Initialization
    //--------------------------------------------------------------------------------------
    Vector2 screenSize = (Vector2){800, 800};

    World currentWorld;
    initWorld(&currentWorld, RectangleFromSize(screenSize), 200.0);

    InitWindow(screenSize.x, screenSize.y, "raylib [core] example - basic window");

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) {   // Detect window close button or ESC key
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------
        
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 spawnSize = (Vector2){20, 20};
            Vector2 spawnTopLeft = Vector2Subtract(GetMousePosition(), Vector2Scale(spawnSize, 0.5));
            for ITERATE(x, spawnSize.x)
            for ITERATE(y, spawnSize.y) {
                spawnBoid(&currentWorld, Vector2Add(spawnTopLeft, (Vector2){x, y}));
            }
            
        }

        clearBoidMap(&currentWorld.boidMap);
        populateBoidMap(&currentWorld.boidMap, &currentWorld.boids);
        boidsReact(&currentWorld.boids, &currentWorld.boidMap, currentWorld.waterBounds, currentWorld.snake.position, FIXED_DELTA);
        boidsMove(&currentWorld.boids, currentWorld.waterBounds, FIXED_DELTA);

        snakeUpdate(&currentWorld.snake, FIXED_DELTA);
        snakeMove(&currentWorld.snake, FIXED_DELTA);
        
        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(BLACK);

            boidsRender(&currentWorld.boids);
            snakeRender(&currentWorld.snake);

            //DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
