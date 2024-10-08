#include <stdlib.h>
#include <assert.h>
#include <stdint.h>
#include "raylib.h"
#include "game.h"
#include "array.h"
#include "hash_table.h"
#include "raymath.h"

//#define _CRT_SECURE_NO_WARNINGS


#ifndef _DEBUG


#pragma comment(linker, "/SUBSYSTEM:windows /ENTRY:mainCRTStartup")

#endif


#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION            330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION            100
#endif



#define ITERATE(IDX, LEN) (size_t IDX = 0; IDX < LEN; IDX++)
#define DEBUG_ENABLED true
#define CHUNK_SIZE (Vector2){16, 16}
#define FPS 60
#define FIXED_DELTA 1.0/FPS
#define BOID_CHUNK_MAX 32

bool gameHasStarted = false;
float comboFlashPercentage = 0.0;

////////////////////////////////////////////
// ..Enum
////////////////////////////////////////////



////////////////////////////////////////////
// ..Header
////////////////////////////////////////////

enum Flag {
    FLAG_DEAD = 1,
    FLAG_SPAWNING = 2,
    FLAG_DROPPED = 4,
    FLAG_CAN_DASH = 8,
};

typedef struct Entity {
    uint32_t flags;
    Vector2 position;
    Vector2 velocity;
} Entity;

typedef struct Boid {
    Entity entity;
} Boid;

typedef struct BoidMap {
    Array chunks; //BoidChunk
    Vector2 chunkCount;
} BoidMap;

typedef struct BoidChunk {
    Boid* boids[BOID_CHUNK_MAX];
    int boidCount;
} BoidChunk;

typedef struct SnakeNode {
    Vector2 position;
    float radius;
    float bonusRadius;
} SnakeNode;

typedef struct Snake {
    Entity entity;
    Array nodes;
    float boostPercent;
    float boostColdownTimer;
    float score;
    float comboLevel;
    float comboHealth;
    float rotation;
    float lastBoidEatenAt;
    float clawStretch;
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

typedef struct SplashParticle {
    Entity entity;
    float radius;
} SplashParticle;

typedef struct BloodParticle {
    Entity entity;
    float lifetime;
} BloodParticle;

typedef struct BoidBomb {
    Entity entity;
    unsigned int boidCount;
    float lifetime;
} BoidBomb;

typedef struct World {
    Array boids;
    Array boidBombs;
    Array splashParticles;
    Array bloodParticles;
    BoidMap boidMap;
    Snake snake;
    Rectangle bounds;
    WaterBody water;
    float boidBombSpawnTime;
} World;


////////////////////////////////////////////
// ..Color
////////////////////////////////////////////

Color COLOR_MAIN = WHITE;
Color COLOR_BG = BLACK;  
Color COLOR_HIGHLIGHT = RED;

// Blend Color
Color ColorLerp(Color c1, Color c2, float amount) {
    return (Color) {
        Lerp(c1.r, c2.r, amount),
        Lerp(c1.g, c2.g, amount),
        Lerp(c1.b, c2.b, amount),
        Lerp(c1.a, c2.a, amount),
    };
}

////////////////////////////////////////////
// ..Math
////////////////////////////////////////////

Vector2 getSquashScale(float t, float scaler) {
    static float squashMax = 1.1;
    float squashAmount = 1 - exp( -t * 10);
    return (Vector2){ Lerp(squashMax * scaler, 1.0, squashAmount), Lerp(1.0 / squashMax / scaler, 1.0, squashAmount) };
}

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

float RandFloat() {
    return ((float) rand()) / RAND_MAX;
}

float RandRange(float range_min, float range_max) {
    return Lerp(range_min, range_max, RandFloat());
}

bool RandBool() {
    return rand() & 1;
}

float fsign(float num) {
    if (num > 0) return 1;
    if (num < 0) return -1;
    return 0;
}



////////////////////////////////////////////
// ..Draw
////////////////////////////////////////////

// Draw anchored
void DrawSpriteAnchored(Texture2D texture, Vector2 position, float rotation, Vector2 anchor, Color tint) {
    DrawTexturePro(texture, (Rectangle) { 0, 0, texture.width, texture.height }, 
                            (Rectangle) { position.x, position.y, texture.width, texture.height },
                    Vector2Multiply(anchor, (Vector2){ texture.width , texture.height}), rotation, tint
    );
}

void DrawSpriteAnchoredScaled(Texture2D texture, Vector2 position, float rotation, Vector2 scale, Vector2 anchor, Color tint) {

    if (scale.x < 0) anchor.x = 1.0 - anchor.x;
    if (scale.y < 0) anchor.y = 1.0 - anchor.y;

    Vector2 origin = Vector2Multiply(anchor, (Vector2) { texture.width*fabs(scale.x), texture.height * fabs(scale.y) });
    DrawTexturePro(texture, (Rectangle) {0, 0, texture.width * fsign(scale.x), texture.height * fsign(scale.y)},
        (Rectangle) {
        position.x, 
        position.y, 
        texture.width * fabs(scale.x), texture.height  * fabs(scale.y)
    },
        origin, rotation, tint
    );
}

Vector2 DrawTextAnchored(Vector2 position, Vector2 anchor, Font font, const char* text, int fontSize, float spacing, Color color) {
    Vector2 textSize = MeasureTextEx(font, text, fontSize, spacing);
    Vector2 drawPosition = Vector2Subtract(position, Vector2Multiply(textSize, anchor));
    DrawTextEx(font, text, drawPosition, fontSize, spacing, color);
    return textSize;
}


////////////////////////////////////////////
// ..Camera
////////////////////////////////////////////

Camera2D camera = {0};
float shakeStrength = 0.0;
Vector2 shakeOffset = {0, 0};
Vector2 cameraCenter = { 0, 0 };

void shakeCamera(float newIntensity) {
    shakeStrength = fmaxf(newIntensity, shakeStrength);
}


////////////////////////////////////////////
// ..Sound
////////////////////////////////////////////



#define SOUND_INSTANCE_COUNT 1000

Sound soundInstances[SOUND_INSTANCE_COUNT];

float placeSoundCooldown = 0.0;
bool isSoundOn = true;

bool playSoundInstance(Sound sound, float volume, float pitch) {
    static int lastPlayedSoundInt = 0;
    for (int i = (lastPlayedSoundInt + 1) % SOUND_INSTANCE_COUNT;
        i != lastPlayedSoundInt; i = (i + 1) % SOUND_INSTANCE_COUNT)
    {
        assert(i >= 0);
        assert(i < SOUND_INSTANCE_COUNT);
        Sound currentSound = soundInstances[i];
        if (!IsSoundPlaying(currentSound))
        {
            UnloadSoundAlias(currentSound);
            soundInstances[i] = LoadSoundAlias(sound);
            SetSoundVolume(soundInstances[i], volume);
            SetSoundPitch(soundInstances[i], pitch);
            PlaySound(soundInstances[i]);
            lastPlayedSoundInt = i;
            return true;
        }
        
    }
    return false;
}


Sound HIT_EDGE_SOUND;
Sound LEVEL_RESET_SOUND;
Sound LEVEL_UP_SOUND;
Sound PLANE_SOUND;
Sound SPLASH_IN_SOUND;
Sound SPLASH_OUT_SOUND;
Sound BOMB_EXPLODE_SOUND;
Sound EAT_SOUND;
Sound EAT2_SOUND;
Sound DASH_SOUND;
Sound DASH_FAIL_SOUND;
Sound CAN_DASH_SOUND;


void loadSounds() {
    HIT_EDGE_SOUND = LoadSound("assets/sounds/hit_edge.wav");
    LEVEL_RESET_SOUND = LoadSound("assets/sounds/level_reset.wav");
    LEVEL_UP_SOUND = LoadSound("assets/sounds/level_up.wav");
    PLANE_SOUND = LoadSound("assets/sounds/plane.wav");
    SPLASH_IN_SOUND = LoadSound("assets/sounds/splash_in.wav");
    SPLASH_OUT_SOUND = LoadSound("assets/sounds/splash_out.wav");
    BOMB_EXPLODE_SOUND = LoadSound("assets/sounds/bomb_explode.wav");
    EAT_SOUND = LoadSound("assets/sounds/eat.wav");
    EAT2_SOUND = LoadSound("assets/sounds/eat2.wav");
    DASH_SOUND = LoadSound("assets/sounds/dash.wav");
    DASH_FAIL_SOUND = LoadSound("assets/sounds/dash_fail.wav");
    CAN_DASH_SOUND = LoadSound("assets/sounds/can_dash.wav");

    for ITERATE(i, SOUND_INSTANCE_COUNT) {
        soundInstances[i] = LoadSoundAlias(EAT_SOUND);
    }
}



////////////////////////////////////////////
// ..Sprites
////////////////////////////////////////////

Texture2D PLANE_SPRITE;
Texture2D MANDIBLE_SPRITE;
Texture2D STRING_SPRITE;
Texture2D FISH_ICON_SPRITE;
Image FISH_WINDOW_ICON;

void loadSprites() {
    PLANE_SPRITE        = LoadTexture("assets/sprites/plane.png");
    MANDIBLE_SPRITE     = LoadTexture("assets/sprites/mandible.png");
    STRING_SPRITE       = LoadTexture("assets/sprites/string.png");
    FISH_ICON_SPRITE    = LoadTexture("assets/sprites/fish_icon.png");
    FISH_WINDOW_ICON    = LoadImage("assets/sprites/fish_window_icon.png");
}

////////////////////////////////////////////
// ..Fonts
////////////////////////////////////////////

Font MAIN_FONT;

void loadFonts() {
    MAIN_FONT           = LoadFontEx("assets/fonts/Concert_One/ConcertOne-Regular.ttf", 400, NULL, 0);
}

////////////////////////////////////////////
// ..Shaders
////////////////////////////////////////////

typedef struct GLSLColor {
    float r;
    float g;
    float b;
    float a;
} GLSLColor;

GLSLColor glslColor(Color color) {
    return (GLSLColor) {
        ((float) color.r) /255,
        ((float) color.g) /255,
        ((float) color.b) /255,
        ((float) color.a) /255,
    };
}

Shader MASK_SHADER;

void loadShaders() {
    MASK_SHADER           = LoadShader(0, TextFormat("assets/shaders/mask.fs", GLSL_VERSION));
}


////////////////////////////////////////////
// ..Entity
////////////////////////////////////////////

#define GRAVITY 600

void clearDeadEntities(Array* entities) {
    size_t aliveBoidCount = 0;
    for ITERATE(i, entities->used) {
        Entity* entity = aGet(entities, i);
        bool isDead = (entity->flags & FLAG_DEAD) != 0;

        if (!isDead)
            aSet(entities, aliveBoidCount, entity);

        aliveBoidCount += !isDead;
    }

    entities->used = aliveBoidCount;
}

////////////////////////////////////////////
// ..Water
////////////////////////////////////////////

#define WATER_SPRING_CONSTANT 150
#define WATER_DAMPENING 0.020

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
        waterNode->velocityY = 0;
        waterNode->forceOffsetY = 0;
        waterNode->naturalOffsetY = 0;
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

    DrawSplineLinear(splinePositions.array, splinePositions.used, 5, COLOR_MAIN);
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
// ..SplashParticle
////////////////////////////////////////////

void spawnSpawnParticles(Array* splashParticles, Vector2 position, float speed, unsigned int count) {
    for ITERATE(i, count) {
        SplashParticle splashParticle = {0};
        splashParticle.entity.position = position;
        splashParticle.radius = RandRange(1, 6);
        splashParticle.entity.velocity = Vector2Scale(Vector2Rotate((Vector2){0, -1}, RandRange(-PI / 4, PI / 4)), speed * RandRange(0.8, 1.0) / sqrt(splashParticle.radius));
        splashParticle.entity.flags = FLAG_SPAWNING;
        aAppend(splashParticles, &splashParticle);
    }
}

void splashParticlesMove(Array* splashParticles, WaterBody* water, float delta) {
    
    for ITERATE(i, splashParticles->used) {
        SplashParticle* splashParticle = aGet(splashParticles, i);
        splashParticle->entity.velocity.y += GRAVITY * delta;
        splashParticle->entity.position = Vector2Add(splashParticle->entity.position, Vector2Scale(splashParticle->entity.velocity, delta));
        // printf("%f\n", splashParticle->entity.velocity.y);

        bool particleInWater = inWater(water, splashParticle->entity.position);
        if (!particleInWater) splashParticle->entity.flags &= ~FLAG_SPAWNING;
        if (particleInWater && !(splashParticle->entity.flags & FLAG_SPAWNING)) {
            splashParticle->entity.flags |= FLAG_DEAD;
        }
    }
}

void splashParticlesRender(Array* splashParticles) {
    
    for ITERATE(i, splashParticles->used) {
        SplashParticle* splashParticle = aGet(splashParticles, i);
        DrawCircle(splashParticle->entity.position.x, splashParticle->entity.position.y, splashParticle->radius, COLOR_MAIN);
    }
}


////////////////////////////////////////////
// ..BloodParticle
////////////////////////////////////////////

#define BLOOD_PARTICLE_MAX_LIFETIME 1.0
#define BLOOD_PARTICLE_DAMPENING 0.01

void spawnBloodParticle(Array* bloodParticles, Vector2 position, Vector2 velocity) {

    BloodParticle bloodParticle = {0};
    bloodParticle.entity.position = position;
    bloodParticle.entity.velocity = velocity;
    bloodParticle.lifetime = 0.0;
    aAppend(bloodParticles, &bloodParticle);
}

void bloodParticlesMove(Array* bloodParticles, WaterBody* water, float delta) {
    
    for ITERATE(i, bloodParticles->used) {
        BloodParticle* bloodParticle = aGet(bloodParticles, i);
        bloodParticle->entity.velocity.y += -GRAVITY * delta * 0.2;
        bloodParticle->entity.velocity = Vector2Scale(Vector2Normalize(bloodParticle->entity.velocity), Vector2Length(bloodParticle->entity.velocity) * pow(BLOOD_PARTICLE_DAMPENING, delta));
        bloodParticle->lifetime += delta;
        bloodParticle->entity.position = Vector2Add(bloodParticle->entity.position, Vector2Scale(bloodParticle->entity.velocity, delta));

        bool particleInWater = inWater(water, bloodParticle->entity.position);
        if (!particleInWater) bloodParticle->entity.flags |= FLAG_DEAD;
        if (bloodParticle->lifetime >= BLOOD_PARTICLE_MAX_LIFETIME) bloodParticle->entity.flags |= FLAG_DEAD;
    }
}

void bloodParticlesRender(Array* bloodParticles) {
    
    for ITERATE(i, bloodParticles->used) {
        BloodParticle* bloodParticle = aGet(bloodParticles, i);
        Color color = COLOR_HIGHLIGHT;
        float percentange = Clamp(( bloodParticle->lifetime / BLOOD_PARTICLE_MAX_LIFETIME), 0.0, 1.0);
        color.a = 255 * (1 - percentange);
        DrawCircle(bloodParticle->entity.position.x, bloodParticle->entity.position.y, 4 * (1 - percentange), color);
    }
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
            if (Vector2Distance(boid->entity.position, position) <= radius) {
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






void clearBoidMap(BoidMap* boidMap) {
    for ITERATE(i, boidMap->chunks.used) {
        BoidChunk* boidChunk = aGet(&boidMap->chunks, i);
        boidChunk->boidCount = 0;
    }
}



void populateBoidMap(BoidMap* boidMap, Array* boids) {
    for ITERATE(i, boids->used) {
        Boid* boid = aGet(boids, i);
        BoidChunk* boidChunk = getChunk(boidMap, getChunkPosition(boid->entity.position));
        
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

void spawnBoid(World* world, Vector2 position, Vector2 velocity) {
    Boid boid;
    boid.entity.position = position;
    boid.entity.velocity = velocity;
    boid.entity.flags = 0 | FLAG_SPAWNING;
    aAppend(&world->boids, &boid);
    //printf("SPAWN BOID\n");
}

Vector2 getRandomBoidVelocity() {
    return  Vector2Rotate((Vector2){Lerp(BOID_MIN_SPEED, BOID_MAX_SPEED, RandFloat()), 0}, RandFloat() * 2 * PI);
}

void boidExplosiveSpawn(World* world, Vector2 position, unsigned int countToSpawn) {
    unsigned int spawnSizeSide = ceil(sqrt(countToSpawn));
    unsigned int countSpawned = 0;

    Vector2 spawnSize = Vector2Side(spawnSizeSide);
    Vector2 spawnTopLeft = Vector2Subtract(position, Vector2Scale(spawnSize, 0.5));
    for ITERATE(x, spawnSize.x)
    for ITERATE(y, spawnSize.y) {
        if (countSpawned >= countToSpawn) return;

        spawnBoid(world, Vector2Add(spawnTopLeft, (Vector2){x, y}), getRandomBoidVelocity());
        countSpawned++;
    }
}


void boidsReact(Array* boids, BoidMap* boidMap, WaterBody* water, Rectangle bounds, Vector2 pointToAvoid, float delta) {

    

    Array flockBoids = aCreate(128, sizeof(Boid*));
    for ITERATE(i, boids->used) {
        Boid* boid = aGet(boids, i);
        if (!inWater(water, boid->entity.position)) {
            boid->entity.velocity.y += GRAVITY * delta;
            continue;
        }

        flockBoids.used = 0;
       
        getBoidsIn(boidMap, boid->entity.position, BOID_VISION_RADIUS, &flockBoids);

        Vector2 separationForce = Vector2Zero();
        Vector2 alignmentForce  = Vector2Zero();
        Vector2 cohesionForce   = Vector2Zero();
        Vector2 avoidanceForce  = Vector2Zero();
        Vector2 wallForce   = Vector2Zero();

        

        if (flockBoids.used) {
            unsigned int boidsUsed = 0;

            Vector2 totalDisplacement = Vector2Zero();
            Vector2 totalVelocity = Vector2Zero();
            Vector2 totalPosition = Vector2Zero();

            for ITERATE(j, flockBoids.used) {
                Boid* flockBoid = *(Boid**) aGet(&flockBoids, j);
                if (flockBoid->entity.flags & FLAG_SPAWNING) continue;

                Vector2 displacement = Vector2Subtract(boid->entity.position, flockBoid->entity.position);
                if (Vector2LengthSqr(displacement) <= BOID_SEPARATION_RADIUS * BOID_SEPARATION_RADIUS) 
                    totalDisplacement = Vector2Add(totalDisplacement, displacement); // separation: s BOID_AVOIDANCE_RADIUS 40teer to avoid crowding local flockmates
                totalVelocity = Vector2Add(totalVelocity, flockBoid->entity.velocity);      // alignment: steer towards the average heading of local flockmates
                totalPosition = Vector2Add(totalPosition, flockBoid->entity.position);      // cohesion: steer to move towards the average position (center of mass) of local flockmates
                boidsUsed++;
            }

            if (boidsUsed) {
                separationForce = Vector2Scale(totalDisplacement, BOID_SEPARATION_C);
                alignmentForce  = Vector2Scale(totalVelocity  , BOID_ALIGNMENT_C / boidsUsed);
                cohesionForce   = Vector2Scale(Vector2Subtract(Vector2Scale(totalPosition, 1.0 / boidsUsed), boid->entity.position), BOID_COHESION_C);
            }
        }
        
        
        if (fabs(boid->entity.position.x - RectangleLeft(bounds)) < BOID_WALL_VISION_RADIUS)   wallForce.x += 1;
        if (fabs(boid->entity.position.x - RectangleRight(bounds)) < BOID_WALL_VISION_RADIUS)  wallForce.x -= 1;
        if (fabs(boid->entity.position.y - RectangleTop(bounds)) < BOID_WALL_VISION_RADIUS)    wallForce.y += 1;
        if (fabs(boid->entity.position.y - RectangleBottom(bounds)) < BOID_WALL_VISION_RADIUS) wallForce.y -= 1;

        wallForce = Vector2Scale(wallForce, BOID_WALL_C);

        Vector2 avoidanceDisplacement = Vector2Subtract(boid->entity.position, pointToAvoid);
        float avoidanceDisplacementLengthSqr = Vector2LengthSqr(avoidanceDisplacement);
        if (avoidanceDisplacementLengthSqr < BOID_AVOIDANCE_RADIUS * BOID_AVOIDANCE_RADIUS)
            avoidanceForce = Vector2Scale(Vector2Normalize(avoidanceDisplacement), BOID_AVOIDANCE_C);

        Vector2 force = Vector2Add(Vector2Add(Vector2Add(Vector2Add(separationForce, alignmentForce), cohesionForce), wallForce), avoidanceForce);

        if (Vector2Length(force)) {
            boid->entity.velocity = Vector2Add(boid->entity.velocity, Vector2Scale(force, delta));
            boid->entity.velocity = Vector2Scale(Vector2Normalize(boid->entity.velocity), Clamp(Vector2Length(boid->entity.velocity), BOID_MIN_SPEED, BOID_MAX_SPEED));
        }
    }

    aFree(&flockBoids);
}

void boidsMove(Array* boids, WaterBody* water, Rectangle outerBounds, Rectangle innerBounds, float delta) {

    Rectangle outerClampBounds = RectangleReduceAll(outerBounds, BOID_RADIUS);
    Rectangle innerClampBounds = RectangleReduceAll(innerBounds, BOID_RADIUS);

    

    for ITERATE(i, boids->used) {
        Boid* boid = aGet(boids, i);
        Rectangle clampBounds = (boid->entity.flags & FLAG_SPAWNING) ? outerClampBounds : innerClampBounds;
        boid->entity.position = Vector2Add(boid->entity.position, Vector2Scale(boid->entity.velocity, delta));
        boid->entity.position = Vector2Clamp(boid->entity.position, RectangleTopLeft(clampBounds), RectangleBottomRight(clampBounds));

        if ((boid->entity.flags & FLAG_SPAWNING) && inWater(water, boid->entity.position)) {
            boid->entity.flags &= ~FLAG_SPAWNING;
            assert(!(boid->entity.flags & FLAG_SPAWNING));
            boid->entity.velocity = getRandomBoidVelocity();
        }
    }

    
}

void boidsRender(Array* boids)
{
    for ITERATE(i, boids->used) {
        Boid* boid = aGet(boids, i);
        DrawCircle(boid->entity.position.x, boid->entity.position.y, BOID_RADIUS, COLOR_MAIN);
    }
}


////////////////////////////////////////////
// ..BoidBomb
////////////////////////////////////////////

#define PLANE_PUT_PERIOD 0.5

float getBoidBombRadius(unsigned int boidCount) {
    return sqrt(boidCount);
}


void spawnBoidBomb(World* world, Vector2 position, Vector2 velocity, float time, unsigned int boidCount) {
    BoidBomb boidBomb = {0};
    boidBomb.entity.position = position;
    boidBomb.entity.velocity = velocity;
    boidBomb.entity.flags = 0 | FLAG_SPAWNING;
    boidBomb.boidCount = boidCount;
    boidBomb.lifetime = 0;
    aAppend(&world->boidBombs, &boidBomb);
}

void boidBombsMove(Array* boidBombs, Rectangle bounds, float delta) {

    

    for ITERATE(i, boidBombs->used) {
        BoidBomb* boidBomb = aGet(boidBombs, i);

        float oldLifetime = boidBomb->lifetime;
        boidBomb->lifetime += delta;

        if (fmod(oldLifetime, PLANE_PUT_PERIOD) > fmod(boidBomb->lifetime, PLANE_PUT_PERIOD)) {
            playSoundInstance(PLANE_SOUND, 0.6, 1.0);
        }

        float radius = getBoidBombRadius(boidBomb->boidCount);
        boidBomb->entity.position = Vector2Add(boidBomb->entity.position, Vector2Scale(boidBomb->entity.velocity, delta));

        bool inBounds = CheckCollisionCircleRec(boidBomb->entity.position, radius, bounds);
        bool isSpawning = boidBomb->entity.flags & FLAG_SPAWNING;

        if (inBounds) boidBomb->entity.flags &= ~FLAG_SPAWNING;
        if (!inBounds && !isSpawning) boidBomb->entity.flags |= FLAG_DEAD;
    }    
}

void boidBombsRender(Array* boidBombs, float time) {
    for ITERATE(i, boidBombs->used) {
        BoidBomb* boidBomb = aGet(boidBombs, i);
        float radius = getBoidBombRadius(boidBomb->boidCount);
        int facing = boidBomb->entity.velocity.x > 0 ? 1 : -1;
        const float planeOffsetY = -15;

        Vector2 squash = getSquashScale(fmod(boidBomb->lifetime, PLANE_PUT_PERIOD), 1.05);

        DrawSpriteAnchoredScaled(PLANE_SPRITE, Vector2Add(boidBomb->entity.position, (Vector2){0, -radius + planeOffsetY}), 0, Vector2Multiply(squash, (Vector2){facing, 1}), (Vector2){0.5, 0.5}, COLOR_MAIN);
        if (!(boidBomb->entity.flags & FLAG_DROPPED)) {
            DrawSpriteAnchoredScaled(STRING_SPRITE, Vector2Add(boidBomb->entity.position, (Vector2){0, -radius + planeOffsetY + -5}), 0, Vector2Scale(Vector2One(), radius * 2 / 50.0), (Vector2){0.5, 0.0}, COLOR_MAIN);
            DrawCircle(boidBomb->entity.position.x, boidBomb->entity.position.y, radius, COLOR_MAIN);
            DrawSpriteAnchoredScaled(FISH_ICON_SPRITE, boidBomb->entity.position, 0, Vector2Scale(Vector2One(), radius * 2 / 200.0 * 0.6), (Vector2){0.5, 0.5},  COLOR_BG);
        }
    }
}

void popBoidBomb(BoidBomb* boidBomb, World* world) {
    assert(!(boidBomb->entity.flags & FLAG_DROPPED));
    playSoundInstance(BOMB_EXPLODE_SOUND, 1.5, RandRange(0.9, 1.1));
    playSoundInstance(HIT_EDGE_SOUND, 1.0, RandRange(0.9, 1.1));
    boidBomb->entity.flags |= FLAG_DROPPED;
    boidExplosiveSpawn(world, boidBomb->entity.position, boidBomb->boidCount);
}


////////////////////////////////////////////
// ..Snake
////////////////////////////////////////////

#define SNAKE_MAX_SPEED 400
#define SNAKE_ACCELERATION 1200
#define SNAKE_HITBOX_RADIUS 20
#define SNAKE_BOOST_MAX_SPEED 600
#define SNAKE_HEAD_RADIUS 20
#define SNAKE_TAIL_RADIUS 8
#define SNAKE_BOOST_ACCELERATION SNAKE_BOOST_MAX_SPEED * FPS
#define SNAKE_FRICTION_C 0.02
#define SNAKE_BOUNCE_DAMPENING 0.6
#define SNAKE_BOOST_COOLDOWN_TIME 1.0
#define SNAKE_NODE_COUNT 5

float getComboBonus(float comboLevel) {
    return 1 << (int) (floor(comboLevel));
}

void initSnake(Snake* snake, Vector2 position, Vector2 velocity) {
    snake->entity.position = position;
    snake->entity.velocity = velocity;
    snake->boostPercent = 0.0;
    snake->boostColdownTimer = 0.0;
    snake->score = 0.0;
    snake->comboLevel = 0.0;
    snake->comboHealth = 1.0;
    snake->nodes = aCreate(SNAKE_NODE_COUNT, sizeof(SnakeNode));
    snake->nodes.used = snake->nodes.size;
    snake->rotation = 0.0;
    snake->lastBoidEatenAt = 0.0;

    for ITERATE(i, snake->nodes.used) {
        SnakeNode* snakeNode = aGet(&snake->nodes, i);
        snakeNode->radius = Lerp(SNAKE_HEAD_RADIUS, SNAKE_TAIL_RADIUS, ((float) i) / (snake->nodes.used - 1));
        
        if (i) {
            SnakeNode* previousSnakeNode = aGet(&snake->nodes, i - 1);
            snakeNode->position = Vector2Subtract( previousSnakeNode->position, (Vector2){previousSnakeNode->radius + snakeNode->radius, 0});
        } else {
            snakeNode->position = snake->entity.position;
        }
    }
}

void snakeUpdate(Snake* snake, WaterBody* water, float delta) {
    assert(delta >= 0);

    bool couldDash = snake->boostColdownTimer <= 0.0;

    snake->boostColdownTimer -= delta;
    snake->boostColdownTimer = Clamp( snake->boostColdownTimer, 0.0, SNAKE_BOOST_COOLDOWN_TIME);

    bool canDash = snake->boostColdownTimer <= 0.0;

    if (canDash && !couldDash) {
        playSoundInstance(CAN_DASH_SOUND, 0.5, 1.0);
    }

    if (!inWater(water, snake->entity.position)) {
        // OUT OF WATER
        snake->entity.velocity.y += GRAVITY * delta;
    } else {
        // IN WATER
        Vector2 targetDirection = Vector2Normalize((Vector2) {
            IsKeyDown(KEY_D) - IsKeyDown(KEY_A),
            IsKeyDown(KEY_S) - IsKeyDown(KEY_W),
        });

        
        
        
        
        if (IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_SPACE)) {
            if (canDash) {
                // boost
                snake->entity.velocity = Vector2Scale(targetDirection, SNAKE_BOOST_MAX_SPEED);
                snake->boostColdownTimer = SNAKE_BOOST_COOLDOWN_TIME;
                snake->clawStretch += 10;
                
                gameHasStarted = true;
                playSoundInstance(DASH_SOUND, 0.4, RandRange(0.9, 1.1) * 0.6);
                canDash = false;

                shakeCamera(7.0);
            } else {
                playSoundInstance(DASH_FAIL_SOUND, 1.0, RandRange(0.9, 1.1) * 0.6);
                snake->boostPercent = 0.0;
            }
        } else {
            snake->boostPercent = 0.0;
        }

        
        

        float maxSpeed =        Lerp(SNAKE_MAX_SPEED    , SNAKE_BOOST_MAX_SPEED     , snake->boostPercent);
        float acceleration =    Lerp(SNAKE_ACCELERATION , SNAKE_BOOST_ACCELERATION  , snake->boostPercent);

        Vector2 targetVelocity = Vector2Scale(targetDirection, fmax(maxSpeed, Vector2Length(snake->entity.velocity)));
        Vector2 velocityDifference = Vector2Subtract(targetVelocity, snake->entity.velocity);
        Vector2 accelerationDirection = Vector2Normalize(velocityDifference);
        snake->entity.velocity = Vector2Add(snake->entity.velocity, Vector2ClampValue(Vector2Scale(accelerationDirection, acceleration * delta), 0, Vector2Length(velocityDifference)));
        snake->entity.velocity = Vector2Add(snake->entity.velocity, Vector2Scale(snake->entity.velocity, -SNAKE_FRICTION_C * delta));
        snake->entity.velocity = Vector2ClampValue(snake->entity.velocity, 0, fmax(maxSpeed, Vector2Length(snake->entity.velocity)));
    }

    if (Vector2Length(snake->entity.velocity) > 0) {
        snake->rotation = atan2(snake->entity.velocity.y, snake->entity.velocity.x);
    }

    if (canDash)    snake->entity.flags |= FLAG_CAN_DASH;
    else            snake->entity.flags &= ~FLAG_CAN_DASH;
}

void snakeMove(Snake* snake, Rectangle bounds, Array* splashParticles, WaterBody* water, float time, float delta) {
    Vector2 oldPosition = snake->entity.position;

    // Move
    snake->entity.position = Vector2Add(snake->entity.position, Vector2Scale(snake->entity.velocity, delta));
    
    // Bounce of bounds
    Rectangle clampBounds = RectangleReduceAll(bounds, SNAKE_HEAD_RADIUS);

    float edgeHitSpeed = 0.0;
    if (snake->entity.position.x < RectangleLeft(clampBounds))    { 
        edgeHitSpeed = fmax(edgeHitSpeed, abs(snake->entity.velocity.x)); 
        snake->entity.velocity.x = abs(snake->entity.velocity.x)  * SNAKE_BOUNCE_DAMPENING; 
    }
    if (snake->entity.position.x >= RectangleRight(clampBounds))  { 
        edgeHitSpeed = fmax(edgeHitSpeed, abs(snake->entity.velocity.x)); 
        snake->entity.velocity.x = -abs(snake->entity.velocity.x) * SNAKE_BOUNCE_DAMPENING; 
    }
    if (snake->entity.position.y < RectangleTop(clampBounds))     { 
        edgeHitSpeed = fmax(edgeHitSpeed, abs(snake->entity.velocity.y)); 
        snake->entity.velocity.y = abs(snake->entity.velocity.y)  * SNAKE_BOUNCE_DAMPENING; 
    }
    if (snake->entity.position.y >= RectangleBottom(clampBounds)) { 
        edgeHitSpeed = fmax(edgeHitSpeed, abs(snake->entity.velocity.y)); 
        snake->entity.velocity.y = -abs(snake->entity.velocity.y) * SNAKE_BOUNCE_DAMPENING; 
    }

    if (edgeHitSpeed > 0) {
        playSoundInstance(HIT_EDGE_SOUND, Remap(edgeHitSpeed, 0, SNAKE_MAX_SPEED, 0.3, 1.0), RandRange(0.5, 0.6));
    }

    // Update in water
    bool wasInWater = inWater(water, oldPosition);
    bool nowInWater = inWater(water, snake->entity.position);

    if (wasInWater != nowInWater) {
        WaterNode* waterNode = getNearestWaterNode(water, snake->entity.position);
        waterNode->velocityY = snake->entity.velocity.y * 1.6;

        Vector2 spawnPosition = wasInWater ? snake->entity.position : oldPosition;

        spawnSpawnParticles(splashParticles, Vector2Add(spawnPosition, (Vector2) {-1, 0} ), sqrt(abs(snake->entity.velocity.y)) * 20, Clamp(Remap(abs(snake->entity.velocity.y), 0, SNAKE_MAX_SPEED, 10, 60), 10, 60));
        float loudness = Remap(abs(snake->entity.velocity.y), 0, SNAKE_MAX_SPEED, 0.3, 0.8);
        if (wasInWater) {
            playSoundInstance(SPLASH_OUT_SOUND, loudness, RandRange(0.9, 1.1));
        } else {
            playSoundInstance(SPLASH_IN_SOUND, loudness, RandRange(0.9, 1.1));
        }

        shakeCamera(loudness * 10.0f);

        // printf("SPLASH\n");
    }

    // Update nodes
    SnakeNode* snakeNode = aGet(&snake->nodes, 0);
    snakeNode->position = snake->entity.position;

    float scale = getSquashScale(time - snake->lastBoidEatenAt, 1.05).x;
    snakeNode->bonusRadius = (scale - 1) * snakeNode->radius;

    for ITERATE(i, snake->nodes.used - 1) {
        SnakeNode* snakeNode = aGet(&snake->nodes, i);
        SnakeNode* nextSnakeNode = aGet(&snake->nodes, i + 1);
        nextSnakeNode->position = Vector2Add(snakeNode->position, Vector2Scale(Vector2Normalize(Vector2Subtract(nextSnakeNode->position, snakeNode->position)), nextSnakeNode->radius + snakeNode->radius));
        nextSnakeNode->bonusRadius = Lerp(nextSnakeNode->bonusRadius, snakeNode->bonusRadius, delta * 4);
    }

    snake->clawStretch = Lerp(snake->clawStretch, 0, delta * 3);
    snake->clawStretch = Clamp(snake->clawStretch, 0, 15);
}

void snakeRender(Snake* snake, float time) {

    Color bodyColor = (snake->entity.flags & FLAG_CAN_DASH) ? COLOR_MAIN : ColorLerp(COLOR_MAIN, COLOR_BG, 0.1);

    for ITERATE(i, snake->nodes.used - 1) {
        SnakeNode* snakeNode = aGet(&snake->nodes, i + 1);
        float renderRadius = snakeNode->radius + snakeNode->bonusRadius;
        DrawCircle(snakeNode->position.x, snakeNode->position.y, renderRadius, bodyColor);
        DrawCircle(snakeNode->position.x, snakeNode->position.y, renderRadius - Lerp(4, 12, pow(snake->boostColdownTimer / SNAKE_BOOST_COOLDOWN_TIME, 0.3)), COLOR_BG);
    }
    
    SnakeNode* snakeNode = aGet(&snake->nodes, 0);
    float renderRadius = snakeNode->radius + snakeNode->bonusRadius;
    DrawCircle(snake->entity.position.x, snake->entity.position.y, renderRadius, bodyColor);
    DrawSpriteAnchoredScaled(MANDIBLE_SPRITE, snake->entity.position, RAD2DEG * snake->rotation - snake->clawStretch * 2, (Vector2){1, 1}, (Vector2){0, 1.2}, bodyColor);
    DrawSpriteAnchoredScaled(MANDIBLE_SPRITE, snake->entity.position, RAD2DEG * snake->rotation + snake->clawStretch * 2, (Vector2){1, -1}, (Vector2){0, 1.2}, bodyColor);
}

void snakeEat(Snake* snake, World* world, Array* boids, Array* boidBombs, Array* bloodParticles, float time, float delta) {

    Vector2 snakeDirection = Vector2Normalize(snake->entity.velocity);
    Vector2 hitboxPosition = Vector2Add(snake->entity.position, Vector2Scale(snakeDirection, 10));
    unsigned int boidsEaten = 0;

    for ITERATE(i, boids->used) {
        Boid* boid = aGet(boids, i);
        bool isDead = (boid->entity.flags & FLAG_DEAD);
        bool isSpawning = (boid->entity.flags & FLAG_SPAWNING);
        bool isEaten = !isSpawning && !isDead && CheckCollisionCircles(boid->entity.position, BOID_RADIUS, hitboxPosition, SNAKE_HITBOX_RADIUS);

        if (isEaten) {
            boid->entity.flags |= FLAG_DEAD;
            boidsEaten += 1;
            spawnBloodParticle(bloodParticles, boid->entity.position, Vector2Scale(Vector2Normalize(Vector2Subtract(boid->entity.position, snake->entity.position)), Vector2Length(snake->entity.velocity)));
        }
    }

    for ITERATE(i, boidBombs->used) {
        BoidBomb* boidBomb = aGet(boidBombs, i);
        if (boidBomb->entity.flags & FLAG_DROPPED) continue;
        if (!CheckCollisionCircles(hitboxPosition, SNAKE_HITBOX_RADIUS, boidBomb->entity.position, getBoidBombRadius(boidBomb->boidCount))) continue;
        popBoidBomb(boidBomb, world);
        snake->clawStretch += 10;
        snake->comboHealth += 0.5;
        comboFlashPercentage = 1.0;
        shakeCamera(10.0);
    }

    

    float oldComboLevel = snake->comboLevel;
    snake->score += boidsEaten * getComboBonus(snake->comboLevel);
    snake->comboLevel += boidsEaten / (floor(snake->comboLevel) + 1) * 0.03;

    if (floor(oldComboLevel) < floor(snake->comboLevel)) {
        playSoundInstance(LEVEL_UP_SOUND, 1.0, 1.0);
        snake->comboHealth = 1.0;
        comboFlashPercentage = 1.0;
    }

    snake->comboHealth -= delta * 0.2 * (floor(snake->comboLevel) > 0);

    snake->comboHealth = Clamp( snake->comboHealth, 0.0, 1.0);
    // Reset
    if (snake->comboHealth <= 0) {
        playSoundInstance(LEVEL_RESET_SOUND, 1.0, 1.0);
        snake->comboLevel = 0.0;
        snake->comboHealth = 1.0;
    }

    if (boidsEaten) {
        snake->lastBoidEatenAt = time;
        snake->clawStretch += boidsEaten * 0.5;
        playSoundInstance(EAT_SOUND, 0.5, Lerp(0.5, 1.0, snake->comboLevel - floor(snake->comboLevel)));
        playSoundInstance(EAT2_SOUND, 0.7, 1.5);
        shakeCamera(2.0);
    }

    
}








////////////////////////////////////////////
// ..World
////////////////////////////////////////////

void initWorld(World* world, Rectangle bounds, float waterLine)
{
    world->boids = aCreate(128, sizeof(Boid));
    world->boidBombs = aCreate(8, sizeof(BoidBomb));
    world->splashParticles = aCreate(64, sizeof(SplashParticle));
    world->bloodParticles = aCreate(64, sizeof(BloodParticle));
    initBoidMap(&world->boidMap, Vector2Ceil(Vector2Divide(RectangleSize(bounds), CHUNK_SIZE))); 
    initSnake(&world->snake, RectangleCenter(bounds), Vector2Zero());

    world->boidBombSpawnTime = 0;
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

 

#define WATER_LINE 225.0

//------------------------------------------------------------------------------------
// Program main entry point
//------------------------------------------------------------------------------------
int main(void) {
    // Initialization
    //--------------------------------------------------------------------------------------
    Vector2 screenSize = (Vector2){800, 800};

    
    World currentWorld;
    initWorld(&currentWorld, RectangleFromSize(screenSize), WATER_LINE);

    Array waves = aCreate(2, sizeof(WaterWave));
    {
        WaterWave wave;
        wave = (WaterWave){1.0, 200.0, 2.0, 0.0}; aAppend(&waves, &wave);
        wave = (WaterWave){1.0, 73.0, -3, 0.3}; aAppend(&waves, &wave);
        wave = (WaterWave){0.5, 13.0, 4, -0.3}; aAppend(&waves, &wave);
    }
    float fixedTime = 0.0;

    Color COLOR_DARK = GetColor(0x171738FF);
    Color COLOR_LIGHT = GetColor(0xC1DBE3FF);

    
    COLOR_HIGHLIGHT = GetColor(0xBC4B51FF);

    InitWindow(screenSize.x, screenSize.y, "raylib [core] example - basic window");
    InitAudioDevice();

    loadSprites();
    loadSounds();
    loadFonts();
    loadShaders();

    SetWindowTitle("NOM");
    SetWindowIcon(FISH_WINDOW_ICON);

    camera.target = Vector2Zero();
    camera.offset = Vector2Zero();
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;
    shakeStrength = 0.0f;

    cameraCenter = Vector2Scale(screenSize, 0.5);

    Vector2 comboTextureSize = (Vector2){screenSize.x, 300};
    RenderTexture2D comboTexture = LoadRenderTexture(comboTextureSize.x, comboTextureSize.y);
    

    SetTargetFPS(60);               // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose()) {   // Detect window close button or ESC key
        // Update
        //----------------------------------------------------------------------------------
        // TODO: Update your variables here
        //----------------------------------------------------------------------------------

        // if (IsKeyPressed(KEY_ENTER)) {
        //     shakeStrength = 100.0f;
        // }

        COLOR_BG = (currentWorld.snake.comboLevel >= 10) ? COLOR_LIGHT : COLOR_DARK;
        COLOR_MAIN = (currentWorld.snake.comboLevel >= 10) ? COLOR_DARK : COLOR_LIGHT;

        shakeStrength = Lerp(shakeStrength, 0.0, FIXED_DELTA * 5);
        shakeOffset = (Vector2){ RandRange(-shakeStrength, shakeStrength), RandRange(-shakeStrength, shakeStrength)};
        
        camera.target = (Vector2){RandRange(-shakeStrength, shakeStrength), RandRange(-shakeStrength, shakeStrength)};

        comboFlashPercentage = Lerp(comboFlashPercentage, 0.0, FIXED_DELTA * 5);
        //printf("%f\n", camera.target.x);

        // if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        //     boidExplosiveSpawn(&currentWorld, GetMousePosition(), 400);
        // }
        
        if (gameHasStarted) {
            currentWorld.boidBombSpawnTime -= FIXED_DELTA;
            if (currentWorld.boidBombSpawnTime <= 0.0) {
                currentWorld.boidBombSpawnTime = RandRange(5.0, 8.0);

                if (currentWorld.boids.used < 2000) {
                    bool spawnOnLeft = RandBool();
                    spawnBoidBomb(
                        &currentWorld, 
                        (Vector2) {spawnOnLeft ? -40 : currentWorld.bounds.width + 40, Lerp(40, WATER_LINE - 40, RandFloat())}, 
                        (Vector2) {(spawnOnLeft ? 1 : -1) * Lerp(40, 60, RandFloat()), 0},
                        fixedTime,
                        Lerp(200, 800, RandFloat())
                    );
                }
            }
        }

        clearDeadEntities(&currentWorld.boids);
        clearDeadEntities(&currentWorld.boidBombs);
        clearDeadEntities(&currentWorld.splashParticles);
        clearDeadEntities(&currentWorld.bloodParticles);

        clearBoidMap(&currentWorld.boidMap);
        populateBoidMap(&currentWorld.boidMap, &currentWorld.boids);
        boidsReact(&currentWorld.boids, &currentWorld.boidMap, &currentWorld.water, currentWorld.water.bounds, currentWorld.snake.entity.position, FIXED_DELTA);
        boidsMove(&currentWorld.boids, &currentWorld.water, currentWorld.bounds, currentWorld.water.bounds, FIXED_DELTA);
       


        boidBombsMove(&currentWorld.boidBombs, currentWorld.bounds, FIXED_DELTA);
        

        snakeUpdate(&currentWorld.snake, &currentWorld.water, FIXED_DELTA);
        snakeMove(&currentWorld.snake, currentWorld.bounds, &currentWorld.splashParticles, &currentWorld.water, fixedTime, FIXED_DELTA);
        snakeEat(&currentWorld.snake, &currentWorld, &currentWorld.boids, &currentWorld.boidBombs, &currentWorld.bloodParticles, fixedTime, FIXED_DELTA);

        waterBodyUpdate(&currentWorld.water, FIXED_DELTA);
        waterBodyMove(&currentWorld.water, &waves, fixedTime, FIXED_DELTA);

        splashParticlesMove(&currentWorld.splashParticles, &currentWorld.water, FIXED_DELTA);
        bloodParticlesMove(&currentWorld.bloodParticles, &currentWorld.water, FIXED_DELTA);
        
        // Draw
        //----------------------------------------------------------------------------------
        BeginDrawing(); {
            
            ClearBackground(COLOR_BG);

            

            BeginMode2D(camera); 
                char str[80];

                if (gameHasStarted) {
                    int comboBonus = (int) getComboBonus(currentWorld.snake.comboLevel);
                    sprintf(str, "x%d", (int) getComboBonus(currentWorld.snake.comboLevel));
                    Vector2 drawPosition = RectangleCenter(currentWorld.water.bounds);
                    drawPosition.y += 60;

                    float cutoff = currentWorld.snake.comboLevel - floor(currentWorld.snake.comboLevel);
                    float colorBonus = currentWorld.snake.comboHealth;
                    EndMode2D();
                    BeginTextureMode(comboTexture); {
                        Vector2 scale = getSquashScale((1.0 - comboFlashPercentage) * 0.5, 1.05);

                        ClearBackground((Color){0});
                        Vector2 textSize = DrawTextAnchored( Vector2Scale(comboTextureSize, 0.5), (Vector2){0.5, 0.5}, MAIN_FONT, str, 200 * scale.x, 0.0, WHITE);
                        float half = (1.0 - (textSize.y / comboTextureSize.y)) / 2.0;
                        cutoff = Remap(cutoff, 0.0, 1.0, half + 0.1, 1.0 - half - 0.15);
                    } EndTextureMode();
                    BeginMode2D(camera); 
                    
                    BeginShaderMode(MASK_SHADER); {
                        //DrawTexture(comboTexture.texture, 50, 50, WHITE);
                        
                        

                        SetShaderValue(MASK_SHADER, GetShaderLocation(MASK_SHADER, "cutoff"), &cutoff, SHADER_UNIFORM_FLOAT);
                        

                        GLSLColor color2 = glslColor(ColorLerp(ColorLerp(COLOR_BG, COLOR_MAIN, 0.1 + colorBonus * 0.1), COLOR_MAIN, comboFlashPercentage));
                        GLSLColor color1 = glslColor(ColorLerp(ColorLerp(COLOR_BG, COLOR_MAIN, 0.2 + colorBonus * 0.1), COLOR_MAIN, comboFlashPercentage));

    

                        color1.a = Lerp(0.1, 1.0, currentWorld.snake.comboHealth);
                        color2.a = color1.a;
                        SetShaderValue(MASK_SHADER, GetShaderLocation(MASK_SHADER, "col1"), &color1, SHADER_UNIFORM_VEC4);
                        SetShaderValue(MASK_SHADER, GetShaderLocation(MASK_SHADER, "col2"), &color2, SHADER_UNIFORM_VEC4);

                        

                        DrawTextureRec(comboTexture.texture, (Rectangle){ 0, 0, comboTexture.texture.width, -comboTexture.texture.height}, Vector2Add(Vector2Subtract(drawPosition, Vector2Multiply(comboTextureSize, (Vector2){0.5, 1.0})), ((Vector2){0, 40})), WHITE);
                        // DrawTexturePro(comboTexture.texture, (Rectangle){ 0, 0, comboTexture.texture.width, -comboTexture.texture.height}, 
                        // (Rectangle){ drawPosition.x - comboTexture.texture.width / 2, drawPosition.y - comboTexture.texture.height / 2, comboTexture.texture.width, -comboTexture.texture.height}, 
                        // Vector2Zero(), 0, WHITE);
                    } EndShaderMode();

                    sprintf(str, "%d", (int) floor(currentWorld.snake.score));
                    DrawTextAnchored(drawPosition, (Vector2){0.5, 0.0}, MAIN_FONT, str, 50, 0.0, ColorLerp(COLOR_BG, COLOR_MAIN, 0.1));
                } else {
                    Vector2 drawPosition = RectangleCenter(currentWorld.water.bounds);
                    DrawTextAnchored(drawPosition, (Vector2){0.5, 1.0}, MAIN_FONT, "[WASD] to Move.", 50, 0.0, ColorLerp(COLOR_BG, COLOR_MAIN, 0.3));
                    DrawTextAnchored(drawPosition, (Vector2){0.5, 0.0}, MAIN_FONT, "[Space] to Dash.", 50, 0.0, ColorLerp(COLOR_BG, COLOR_MAIN, 0.3));
                }
                

                bloodParticlesRender(&currentWorld.bloodParticles);
                boidsRender(&currentWorld.boids);
                waterBodyRender(&currentWorld.water);
                boidBombsRender(&currentWorld.boidBombs, fixedTime);
                snakeRender(&currentWorld.snake, fixedTime);
                splashParticlesRender(&currentWorld.splashParticles);
                
            EndMode2D();
        } EndDrawing();
        //----------------------------------------------------------------------------------

        fixedTime += FIXED_DELTA;
    }

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}
