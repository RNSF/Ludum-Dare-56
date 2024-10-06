#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
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

enum Flag {
    FLAG_DEAD = 1,
};

typedef struct Boid {
    uint32_t flags;
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
    float boostPercent;
    float boostColdownTimer;
    float score;
    float comboLevel;
    float comboHealth;
} Snake;

typedef struct WaterNode {
    Vector2 restingPosition;
    float forceOffsetY;
    float naturalOffsetY;
    float velocityY;
} WaterNode;

typedef struct WaterBody {
    Rectangle bounds;
    Array nodes; //WaterNode
} WaterBody;

typedef struct WaterWave {
    float magnitude;
    float period;
    float speed;
    float offset;
} WaterWave;

typedef struct World {
    Array boids;
    BoidMap boidMap;
    Snake snake;
    Rectangle bounds;
    WaterBody water;
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

Rectangle RectangleReduceAll(Rectangle rect, float amount) {
    return (Rectangle){rect.x + amount, rect.y + amount, rect.width - amount * 2, rect.height - amount * 2};
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
// ..Water
////////////////////////////////////////////

#define WATER_SPRING_CONSTANT 150
#define WATER_DAMPENING 0.025

void initWaterBody(WaterBody* water, Rectangle bounds, float nodesPerDistance) {

    unsigned int nodeCount = ceil(bounds.width * nodesPerDistance);
    assert(nodeCount > 1);

    water->nodes = aCreate(nodeCount, sizeof(WaterNode));
    water->nodes.used = water->nodes.size;
    water->bounds = bounds;

    Vector2 boundsTopLeft = RectangleTopLeft(bounds);

    for ITERATE(i, water->nodes.used) {
        WaterNode* waterNode = aGet(&water->nodes, i);
        waterNode->restingPosition = Vector2Add(boundsTopLeft, (Vector2){bounds.width * ((float) i) / (water->nodes.used - 1), 0});
    }
}

void cleanWaterBody(WaterBody* water) {
    aFree(&water->nodes);
}


Vector2 getWaterNodePosition(WaterNode* waterNode) {
    return Vector2Add(waterNode->restingPosition, (Vector2){0, waterNode->forceOffsetY + waterNode->naturalOffsetY});
}

Vector2 getWaterNodeForcedPosition(WaterNode* waterNode) {
    return Vector2Add(waterNode->restingPosition, (Vector2){0, waterNode->forceOffsetY});
}


float waveAt(WaterWave* wave, float x, float t) {
    return wave->magnitude * sin(-wave->speed * t + 2 * PI * (x / wave->period - wave->offset));
}

void waterBodyUpdate(WaterBody* water, float delta) {
    
    for ITERATE(i, water->nodes.used) {
        WaterNode* waterNode = aGet(&water->nodes, i);
        waterNode->velocityY -= WATER_SPRING_CONSTANT * waterNode->forceOffsetY * delta;
        
        Vector2 referencePosition = getWaterNodeForcedPosition(waterNode);

        int connectedNodes[] = {i - 1, i + 1};

        for ITERATE(ji, 2) {
            int j = connectedNodes[ji];

            if (aHas(&water->nodes, j)) {
                WaterNode* otherNode = aGet(&water->nodes, j);
                Vector2 displacement = Vector2Subtract(getWaterNodeForcedPosition(otherNode), referencePosition);
                waterNode->velocityY += WATER_SPRING_CONSTANT * displacement.y  * delta;
            }
        }

        waterNode->velocityY *= pow(WATER_DAMPENING, delta);
        
    }

    
}

void waterBodyMove(WaterBody* water, Array* waves, float time, float delta) {

    for ITERATE(i, water->nodes.used) {
        WaterNode* waterNode = aGet(&water->nodes, i);
        waterNode->forceOffsetY += waterNode->velocityY * delta;

        waterNode->naturalOffsetY = 0.0;

        for ITERATE(j, waves->used) {
            WaterWave* wave = aGet(waves, j);
            waterNode->naturalOffsetY += waveAt(wave, waterNode->restingPosition.x, time);
        }
    }
}

void waterBodyRender(WaterBody* water) {

    Array splinePositions = aCreate(water->nodes.used, sizeof(Vector2));
    splinePositions.used = splinePositions.size;

    for ITERATE(i, water->nodes.used) {
        WaterNode* waterNode = aGet(&water->nodes, i);
        Vector2 position = getWaterNodePosition(waterNode);
        aSet(&splinePositions, i, &position);
    }

    DrawSplineLinear(splinePositions.array, splinePositions.used, 5, WHITE);
    aFree(&splinePositions);
}

WaterNode* getNearestWaterNode(WaterBody* water, Vector2 position) {

    int nodeId = position.x / water->bounds.width * water->nodes.used;
    nodeId = Clamp(nodeId, 0, water->nodes.used - 1);
    WaterNode* waterNode = aGet(&water->nodes, nodeId);
    return waterNode;
}

bool inWater(WaterBody* water, Vector2 position) {
    return CheckCollisionPointRec(position, water->bounds);
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
#define BOID_AVOIDANCE_RADIUS 60
#define BOID_WALL_VISION_RADIUS 100

#define BOID_SEPARATION_C 50
#define BOID_ALIGNMENT_C 50
#define BOID_COHESION_C 0.5
#define BOID_WALL_C 2000.0
#define BOID_AVOIDANCE_C 8000.0

#define BOID_MIN_SPEED 100
#define BOID_MAX_SPEED 200

#define BOID_RADIUS 2



void clearDeadBoids(Array* boids) {
    size_t aliveBoidCount = 0;
    for ITERATE(i, boids->used) {
        Boid* boid = aGet(boids, i);
        bool isDead = (boid->flags & FLAG_DEAD) != 0;

        if (!isDead)
            aSet(boids, aliveBoidCount, boid);

        aliveBoidCount += !isDead;
    }

    boids->used = aliveBoidCount;
}


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
    Boid boid = {0, position, (Vector2){BOID_MAX_SPEED, 0}};
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

    Rectangle clampBounds = RectangleReduceAll(bounds, BOID_RADIUS);

    for ITERATE(i, boids->used) {
        Boid* boid = aGet(boids, i);
        boid->position = Vector2Add(boid->position, Vector2Scale(boid->velocity, delta));
        boid->position = Vector2Clamp(boid->position, RectangleTopLeft(clampBounds), RectangleBottomRight(clampBounds));
    }
}

void boidsRender(Array* boids)
{
    for ITERATE(i, boids->used) {
        Boid* boid = aGet(boids, i);
        DrawCircle(boid->position.x, boid->position.y, BOID_RADIUS, WHITE);
    }
}


////////////////////////////////////////////
// ..Snake
////////////////////////////////////////////

#define SNAKE_MAX_SPEED 400
#define SNAKE_ACCELERATION 800
#define SNAKE_HITBOX_RADIUS 20
#define SNAKE_BOOST_MAX_SPEED 600
#define SNAKE_HEAD_RADIUS 20
#define SNAKE_BOOST_ACCELERATION SNAKE_BOOST_MAX_SPEED * FPS
#define SNAKE_FRICTION_C 0.02
#define GRAVITY 800
#define SNAKE_BOUNCE_DAMPENING 0.6

void snakeUpdate(Snake* snake, WaterBody* water, float delta) {
    assert(delta >= 0);

    if (!inWater(water, snake->position)) {
        snake->velocity.y += GRAVITY * delta;
        return;
    }

    Vector2 targetDirection = Vector2Normalize((Vector2) {
        IsKeyDown(KEY_D) - IsKeyDown(KEY_A),
        IsKeyDown(KEY_S) - IsKeyDown(KEY_W),
    });

    if (IsKeyPressed(KEY_LEFT_SHIFT)) {
        snake->velocity = Vector2Scale(targetDirection, SNAKE_BOOST_MAX_SPEED);
    } else {
        snake->boostPercent = 0.0;
    }
    

    float maxSpeed =        Lerp(SNAKE_MAX_SPEED    , SNAKE_BOOST_MAX_SPEED     , snake->boostPercent);
    float acceleration =    Lerp(SNAKE_ACCELERATION , SNAKE_BOOST_ACCELERATION  , snake->boostPercent);

    Vector2 targetVelocity = Vector2Scale(targetDirection, fmax(maxSpeed, Vector2Length(snake->velocity)));
    Vector2 velocityDifference = Vector2Subtract(targetVelocity, snake->velocity);
    Vector2 accelerationDirection = Vector2Normalize(velocityDifference);
    snake->velocity = Vector2Add(snake->velocity, Vector2ClampValue(Vector2Scale(accelerationDirection, acceleration * delta), 0, Vector2Length(velocityDifference)));
    snake->velocity = Vector2Add(snake->velocity, Vector2Scale(snake->velocity, -SNAKE_FRICTION_C * delta));
    snake->velocity = Vector2ClampValue(snake->velocity, 0, fmax(maxSpeed, Vector2Length(snake->velocity)));
}

void snakeMove(Snake* snake, Rectangle bounds, WaterBody* water, float delta) {
    Vector2 oldPosition = snake->position;
    snake->position = Vector2Add(snake->position, Vector2Scale(snake->velocity, delta));
    Rectangle clampBounds = RectangleReduceAll(bounds, SNAKE_HEAD_RADIUS);

    if (snake->position.x < RectangleLeft(clampBounds))     snake->velocity.x = abs(snake->velocity.x)  * SNAKE_BOUNCE_DAMPENING;
    if (snake->position.x >= RectangleRight(clampBounds))   snake->velocity.x = -abs(snake->velocity.x) * SNAKE_BOUNCE_DAMPENING;
    if (snake->position.y < RectangleTop(clampBounds))      snake->velocity.y = abs(snake->velocity.y)  * SNAKE_BOUNCE_DAMPENING;
    if (snake->position.y >= RectangleBottom(clampBounds))  snake->velocity.y = -abs(snake->velocity.y) * SNAKE_BOUNCE_DAMPENING;

    bool wasInWater = inWater(water, oldPosition);
    bool nowInWater = inWater(water, snake->position);

    if (wasInWater != nowInWater) {
        WaterNode* waterNode = getNearestWaterNode(water, snake->position);
        waterNode->velocityY = snake->velocity.y * 1.2;
        printf("SPLASH\n");
    }
}

void snakeRender(Snake* snake) {
    DrawCircle(snake->position.x, snake->position.y, SNAKE_HEAD_RADIUS, WHITE);
}

void snakeEat(Snake* snake, Array* boids, float delta) {

    Vector2 snakeDirection = Vector2Normalize(snake->velocity);
    Vector2 hitboxPosition = Vector2Add(snake->position, Vector2Scale(snakeDirection, 10));
    unsigned int boidsEaten = 0;

    for ITERATE(i, boids->used) {
        Boid* boid = aGet(boids, i);
        bool isDead = (boid->flags & FLAG_DEAD);
        bool isEaten = !isDead && CheckCollisionCircles(boid->position, BOID_RADIUS, hitboxPosition, SNAKE_HITBOX_RADIUS);

        if (isEaten) {
            boid->flags |= FLAG_DEAD;
            boidsEaten += 1;
        }
    }

    snake->score += boidsEaten * pow(2, floor(snake->comboLevel));
    snake->comboLevel += boidsEaten / (snake->comboLevel + 1) * 0.1;
    snake->comboHealth += boidsEaten * 0.01 / (snake->comboLevel + 1);
    snake->comboHealth -= delta * 0.2 * (snake->comboLevel > 0);

    snake->comboHealth = Clamp( snake->comboHealth, 0.0, 1.0);
    // Reset
    if (snake->comboHealth <= 0) {
        snake->comboLevel = 0.0;
        snake->comboHealth = 1.0;
    }
}







////////////////////////////////////////////
// ..World
////////////////////////////////////////////

void initWorld(World* world, Rectangle bounds, float waterLine)
{
    world->boids = aCreate(128, sizeof(Boid));
    initBoidMap(&world->boidMap, Vector2Ceil(Vector2Divide(RectangleSize(bounds), CHUNK_SIZE)));
    world->snake.position = RectangleCenter(bounds);
    world->snake.boostPercent = 0.0;
    world->snake.boostColdownTimer = 0.0;
    world->snake.score = 0.0;
    world->snake.comboLevel = 0.0;
    world->snake.comboHealth = 1.0;


    world->bounds = bounds;
    Rectangle waterBounds = bounds;
    waterBounds.y += waterLine;
    waterBounds.height -= waterLine;
    initWaterBody(&world->water, waterBounds, 0.1);
}

void cleanWorld(World* world)
{
    aFree(&world->boids);
    cleanBoidMap(&world->boidMap);
    cleanWaterBody(&world->water);
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

    Array waves = aCreate(2, sizeof(WaterWave));
    {
        WaterWave wave;
        wave = (WaterWave){1.0, 200.0, 2.0, 0.0}; aAppend(&waves, &wave);
        wave = (WaterWave){1.0, 73.0, -3, 0.3}; aAppend(&waves, &wave);
        wave = (WaterWave){0.5, 13.0, 4, -0.3}; aAppend(&waves, &wave);
    }
    float fixedTime = 0.0;

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
        

        clearDeadBoids(&currentWorld.boids);
        clearBoidMap(&currentWorld.boidMap);
        populateBoidMap(&currentWorld.boidMap, &currentWorld.boids);
        boidsReact(&currentWorld.boids, &currentWorld.boidMap, currentWorld.water.bounds, currentWorld.snake.position, FIXED_DELTA);
        boidsMove(&currentWorld.boids, currentWorld.water.bounds, FIXED_DELTA);

        snakeUpdate(&currentWorld.snake, &currentWorld.water, FIXED_DELTA);
        snakeMove(&currentWorld.snake, currentWorld.bounds, &currentWorld.water, FIXED_DELTA);
        snakeEat(&currentWorld.snake, &currentWorld.boids, FIXED_DELTA);

        waterBodyUpdate(&currentWorld.water, FIXED_DELTA);
        waterBodyMove(&currentWorld.water, &waves, fixedTime, FIXED_DELTA);
        
        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing();

            ClearBackground(BLACK);

            boidsRender(&currentWorld.boids);
            snakeRender(&currentWorld.snake);
            waterBodyRender(&currentWorld.water);

            char str[8];
            sprintf(str, "%d", (int) floor(currentWorld.snake.score));
            DrawText(str, screenSize.x / 2, 20, 20, WHITE);
            sprintf(str, "%d", (int) floor(currentWorld.snake.comboLevel));
            DrawText(str, screenSize.x / 2, 40, 20, WHITE);
            sprintf(str, "%d%",  (int) (currentWorld.snake.comboHealth * 100));
            DrawText(str, screenSize.x / 2, 60, 20, WHITE);
            //DrawText("Congrats! You created your first window!", 190, 200, 20, LIGHTGRAY);
            DrawFPS(10, 10);
        EndDrawing();
        //----------------------------------------------------------------------------------

        fixedTime += FIXED_DELTA;
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
