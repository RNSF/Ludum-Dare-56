#include <stdlib.h>
#include <assert.h>
#include "raylib.h"
#include "game.h"
#include "array.h"
#include "hash_table.h"
#include "raymath.h"

#define ITERATE(IDX, LEN) (size_t IDX = 0; IDX < LEN; IDX++)
#define DEBUG_ENABLED true
#define CHUNK_SIZE (Vector2){20, 20}
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

typedef struct World {
    Array boids;
    BoidMap boidMap;
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



#define BOID_VISION_RADIUS 100
#define BOID_SEPARATION_C 10
#define BOID_ALIGNMENT_C 10
#define BOID_COHESION_C 10
#define BOID_SPEED 200

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
            printf("Boid outside of world!");
            return;
        }

        if (boidChunk->boidCount >= BOID_CHUNK_MAX) {
            printf("Boid chunk overflow!");;
            return;
        }

        boidChunk->boids[boidChunk->boidCount] = boid;
        boidChunk->boidCount++;
    }
}

void spawnBoid(World* world, Vector2 position) {
    Boid boid = {position, (Vector2){BOID_SPEED, 0}};
    aAppend(&world->boids, &boid);
    printf("SPAWN BOID\n");
}

void boidsReact(Array* boids, BoidMap* boidMap) {

    Array flockBoids = aCreate(128, sizeof(Boid*));
    for ITERATE(i, boids->used) {
        flockBoids.used = 0;
        Boid* boid = aGet(boids, i);
        getBoidsIn(boidMap, boid->position, BOID_VISION_RADIUS, &flockBoids);
        int otherFlockBirdCount = flockBoids.used - 1;

        if (otherFlockBirdCount <= 0) continue;

        Vector2 totalDisplacement = Vector2Zero();
        Vector2 totalVelocity = Vector2Zero();
        Vector2 totalPosition = Vector2Zero();

        for ITERATE(j, flockBoids.used) {
            Boid* flockBoid = *(Boid**) aGet(&flockBoids, j);
            if (flockBoid == boid) continue;

            Vector2 displacement = Vector2Subtract(boid->position, flockBoid->position);
            totalDisplacement = Vector2Add(totalDisplacement, displacement);    // separation: steer to avoid crowding local flockmates
            totalVelocity = Vector2Add(totalVelocity, flockBoid->velocity);     // alignment: steer towards the average heading of local flockmates
            totalPosition = Vector2Add(totalPosition, flockBoid->position);     // cohesion: steer to move towards the average position (center of mass) of local flockmates
        }

        
        Vector2 separationForce = Vector2Scale(totalDisplacement, BOID_SEPARATION_C);
        Vector2 alignmentForce  = Vector2Scale(totalVelocity  , BOID_ALIGNMENT_C / otherFlockBirdCount);
        Vector2 cohesionForce   = Vector2Scale(Vector2Subtract(boid->position, Vector2Scale(totalPosition, 1 / otherFlockBirdCount)), BOID_COHESION_C);

        Vector2 force = Vector2Add(Vector2Add(separationForce, alignmentForce), cohesionForce);
        
        boid->velocity = Vector2Scale(Vector2Normalize(force), BOID_SPEED);
    }

    aFree(&flockBoids);
}

void boidsMove(Array* boids, Rectangle bounds, float delta) {
    for ITERATE(i, boids->used) {
         Boid* boid = aGet(boids, i);
         boid->position = Vector2Add(boid->position, Vector2Scale(boid->velocity, delta));
         while (boid->position.x < 0)               boid->position.x += bounds.width;
         while (boid->position.x >= bounds.width)   boid->position.x -= bounds.width;
         while (boid->position.y < 0)               boid->position.y += bounds.height;
         while (boid->position.y >= bounds.height)  boid->position.y -= bounds.height;
    }
}

void boidsRender(Array* boids)
{
    for ITERATE(i, boids->used) {
        Boid* boid = aGet(boids, i);
        DrawCircle(boid->position.x, boid->position.y, 5, BLACK);
    }
}


////////////////////////////////////////////
// ..World
////////////////////////////////////////////

void initWorld(World* world, Vector2 chunkCount)
{
    world->boids = aCreate(128, sizeof(Boid));
    initBoidMap(&world->boidMap, chunkCount);
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
    const int screenWidth = 800;
    const int screenHeight = 450;

    World currentWorld;
    initWorld(&currentWorld, (Vector2){ceil(screenWidth / CHUNK_SIZE.x), ceil(screenHeight / CHUNK_SIZE.y)});

    InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) {   // Detect window close button or ESC key
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------
        
        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) {
            spawnBoid(&currentWorld, GetMousePosition());
        }

        clearBoidMap(&currentWorld.boidMap);
        populateBoidMap(&currentWorld.boidMap, &currentWorld.boids);
        boidsReact(&currentWorld.boids, &currentWorld.boidMap);
        boidsMove(&currentWorld.boids, (Rectangle){0, 0, screenWidth, screenHeight}, FIXED_DELTA);
        
        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(RAYWHITE);

            boidsRender(&currentWorld.boids);

            DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);

        EndDrawing();
        //----------------------------------------------------------------------------------
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
