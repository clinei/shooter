#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#include <time.h>
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <assert.h>

/*  BUGS

* search for "WRONG"

* search for "trap"

*/

/*  TASKS

* health and player and zombie death

* make zombies keep a certain distance from each other
  to avoid weird behavior when knocked back

* make a separate game clock
  and make everything run in a web worker
  so we can pause and continue

* ammo pickups

* what about using a circular buffer for tables?
  that way we don't have to have a table-> used or check it every iteration

* think about different weapon types

*/

#define PLAYER_SPEED 200
#define BULLET_SPEED 2000
#define FIRING_SPEED 200
#define HIT_FEEDBACK_SPEED 400
#define MAX_FIRING_STATE 100
#define PLAYER_HEALTH 40
#define ZOMBIE_HEALTH 2
#define BULLET_DAMAGE 1
#define BULLET_LIFETIME 5
#define MAX_ENTITY_COUNT 2000
#define PHYSICS_ITER_COUNT 5
#define PHYSICS_BALL_ITER_COUNT 3

typedef unsigned int table_id_t;
typedef unsigned int sprite_id_t;
typedef float sprite_origin_t;
typedef float sprite_size_t;

EMSCRIPTEN_KEEPALIVE
float randf() {
    return rand() / (float)RAND_MAX;
}

typedef unsigned char bool;
enum { false, true };

enum Sprites {
    SPRITE_NONE = 0,
    SPRITE_PLAYER = 1,
    SPRITE_ZOMBIE = 2,
    SPRITE_BULLET = 3
};

struct timespec start_time;
struct timespec curr_time;
struct timespec prev_time;

// stop must be bigger than start
void timespec_diff(const struct timespec* stop,
                   const struct timespec* start,
                         struct timespec* result) {

    if (stop->tv_nsec < start->tv_nsec) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    }
    else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }
}
float timespec_to_float(const struct timespec* spec) {
    // maybe return ms instead of seconds
    return (float)spec->tv_sec + (float)(spec->tv_nsec / 1000000) / 1000;
}
float difftime_float(const struct timespec* stop, const struct timespec* start) {
    struct timespec delta;
    timespec_diff(stop, start, &delta);
    return timespec_to_float(&delta);
}

void begin_time() {
    clock_gettime(CLOCK_REALTIME, &start_time);
}
float step_time() {
    clock_gettime(CLOCK_REALTIME, &curr_time);
    timespec_diff(&curr_time, &start_time, &curr_time);
    const float delta = difftime_float(&curr_time, &prev_time);
    prev_time = curr_time;
    return delta;
}

// table abstraction
// all concrete tables much have these elements first
// so we can use generic functions on them
struct Table {
    // standard array length
    size_t max_count;
    // if the item is not used, we can forego computation
    bool* used;
    // we can keep track of the index of the last item that's used
    // so we can iterate less
    size_t curr_max;
    // global id that's used to join tables
    table_id_t* entity_id;
};
void alloc_table(void* table_ptr, size_t max_count) {
    struct Table* table = (struct Table*)table_ptr;
    table->max_count = max_count;
    table->used = malloc(max_count * sizeof(bool));
    for (table_id_t i = 0; i < max_count; i += 1) {
        table->used[i] = false;
    }
    table->curr_max = 0;
    table->entity_id = malloc(max_count * sizeof(table_id_t));
}
// used for joining tables
// based on their shared index to the entity table
// returns table->curr_max if item not found
EMSCRIPTEN_KEEPALIVE
table_id_t find_item_index(void* table_ptr, table_id_t entity_id) {
    const struct Table* table = (struct Table*)table_ptr;
    table_id_t index = 0;

    // @Audit
    // linear search
    while (index < table->curr_max) {
        if (table->entity_id[index] == entity_id &&
            table->used[index]) {
            break;
        }
        index += 1;
    }

    return index;
}
table_id_t find_first_unused_item(void* table_ptr) {
    struct Table* table = (struct Table*)table_ptr;
    table_id_t index = 0;
    
    // @Audit
    // should this be < instead of <=
    while (index <= table->curr_max) {
        if (table->used[index]) {
            index += 1;
            continue;
        }
        else {
            break;
        }
    }

    return index;
}
// returns table->max_count if table is full
table_id_t add_table_item(void* table_ptr, table_id_t entity_id) {
    struct Table* table = (struct Table*)table_ptr;
    table_id_t index = find_first_unused_item(table);
    if (index == table->curr_max) {
        table->curr_max = index + 1;
    }
    if (index < table->max_count) {
        table->entity_id[index] = entity_id;
        table->used[index] = true;
    }
    return index;
}
void remove_table_item(void* table_ptr, table_id_t entity_id) {
    struct Table* table = (struct Table*)table_ptr;
    const table_id_t index = find_item_index(table_ptr, entity_id);
    table->used[index] = false;
    // if we remove the last item
    // update the curr_max
    while (table->used[table->curr_max - 1] == false && table->curr_max > 0) {
        table->curr_max -= 1;
    }
}

// @Audit
// @Incomplete
// to prevent code drift,
// we should try to use the same functions as the table abstraction
struct Entity_Table {
    size_t max_count;
    bool* used;
    size_t curr_max;
};
struct Entity_Table* entity_table;
void alloc_entity_table(size_t max_count) {
    struct Entity_Table* table = malloc(sizeof(struct Entity_Table));
    entity_table = table;
    table->max_count = max_count;
    table->used = malloc(max_count * sizeof(bool));
    for (table_id_t i = 0; i < max_count; i += 1) {
        table->used[i] = false;
    }
    table->curr_max = 1;
}
table_id_t create_entity() {
    const table_id_t entity_id = find_first_unused_item(entity_table);
    
    if (entity_id == entity_table->curr_max) {
        entity_table->curr_max += 1;
    }
    if (entity_id < entity_table->max_count) {
        entity_table->used[entity_id] = true;
    }
    return entity_id;
}
// the user of a table should remove the entity themself
void remove_entity(table_id_t entity_id) {
    struct Table* table = (struct Table*)entity_table;
    table->used[entity_id] = false;
    while (table->used[table->curr_max - 1] == false && table->curr_max > 0) {
        table->curr_max -= 1;
    }
}

struct Physics_States {
    size_t max_count;
    bool* used;
    size_t curr_max;
    table_id_t* entity_id;
    float* x;
    float* y;
    float* x_speed;
    float* y_speed;
    float* angle;
};
struct Physics_States* physics_states;
void alloc_physics_states(size_t max_count) {
    physics_states = malloc(sizeof(struct Physics_States));
    alloc_table(physics_states, max_count);
    physics_states->x = malloc(max_count * sizeof(float));
    physics_states->y = malloc(max_count * sizeof(float));
    physics_states->x_speed = malloc(max_count * sizeof(float));
    physics_states->y_speed = malloc(max_count * sizeof(float));
    physics_states->angle = malloc(max_count * sizeof(float));
}
table_id_t add_physics_state(table_id_t entity_id,
                             float x,       float y,
                             float x_speed, float y_speed) {

    table_id_t index = add_table_item(physics_states, entity_id);
    if (index < physics_states->max_count) {
        physics_states->x[index] = x;
        physics_states->y[index] = y;
        physics_states->x_speed[index] = x_speed;
        physics_states->y_speed[index] = y_speed;
    }
    return index;
}
void remove_physics_state(table_id_t entity_id) {
    remove_table_item(physics_states, entity_id);
}

EMSCRIPTEN_KEEPALIVE
struct Physics_States* get_physics_states() {
    return physics_states;
}

struct Physics_Balls {
    size_t max_count;
    bool* used;
    size_t curr_max;
    table_id_t* entity_id;
    float* radius;
    float* mass;
};
struct Physics_Balls* physics_balls;
void alloc_physics_balls(size_t max_count) {
    physics_balls = malloc(sizeof(struct Physics_Balls));
    alloc_table(physics_balls, max_count);
    physics_balls->radius = malloc(max_count * sizeof(float));
    physics_balls->mass = malloc(max_count * sizeof(float));
}
table_id_t add_physics_ball(table_id_t entity_id,
                            float radius, float mass) {
    table_id_t index = add_table_item(physics_balls, entity_id);
    if (index < physics_balls->max_count) {
        physics_balls->radius[index] = radius;
        physics_balls->mass[index] = mass;
    }
    return index;
}
void remove_physics_ball(table_id_t entity_id) {
    remove_table_item(physics_balls, entity_id);
}

// because we clear the collision table every frame
// this can be a static table
// with no table->used
// an opportunity to profile
struct Collision_Table {
    size_t max_count;
    bool* used;
    size_t curr_max;
    table_id_t* entity_id;
    table_id_t* entity_id_2;
};
struct Collision_Table* collision_table;
void alloc_collision_table(size_t max_count) {
    collision_table = malloc(sizeof(struct Collision_Table));
    alloc_table(collision_table, max_count);
    collision_table->entity_id_2 = malloc(max_count * sizeof(table_id_t));
}
table_id_t add_collision_item(table_id_t entity_id, table_id_t entity_id_2) {
    table_id_t index = add_table_item(collision_table, entity_id);
    if (index < collision_table->max_count) {
        collision_table->entity_id_2[index] = entity_id_2;
    }
    return index;
}
void clear_collision_table() {
    for (table_id_t i = 0; i < collision_table->curr_max; i += 1) {
        collision_table->used[i] = false;
    }
    collision_table->curr_max = 1;
}

EMSCRIPTEN_KEEPALIVE
struct Collision_Table* get_collision_table() {
    return collision_table;
}

// this could be a static table
struct Hit_Feedback_Table {
    size_t max_count;
    bool* used;
    size_t curr_max;
    table_id_t* entity_id;
    float* amount;
};
struct Hit_Feedback_Table* hit_feedback_table;
void alloc_hit_feedback_table(size_t max_count) {
    hit_feedback_table = malloc(sizeof(struct Hit_Feedback_Table));
    alloc_table(hit_feedback_table, max_count);
    hit_feedback_table->amount = malloc(max_count * sizeof(float));
}
table_id_t add_hit_feedback_item(table_id_t entity_id, float amount) {
    table_id_t index = add_table_item(hit_feedback_table, entity_id);
    if (index < hit_feedback_table->max_count) {
        hit_feedback_table->amount[index] = amount;
    }
    return index;
}
void clear_hit_feedback_table() {
    for (table_id_t i = 0; i < hit_feedback_table->curr_max; i += 1) {
        hit_feedback_table->used[i] = false;
    }
    hit_feedback_table->curr_max = 1;
}

EMSCRIPTEN_KEEPALIVE
struct Hit_Feedback_Table* get_hit_feedback_table() {
    return hit_feedback_table;
}

struct Sprite_Map {
    size_t max_count;
    bool* used;
    size_t curr_max;
    table_id_t* entity_id;
    sprite_id_t*     sprite_id;
    sprite_origin_t* sprite_origin_x;
    sprite_origin_t* sprite_origin_y;
    sprite_size_t*   sprite_size;
};
struct Sprite_Map* sprite_map;
void alloc_sprite_map(size_t max_count) {
    sprite_map = malloc(sizeof(struct Sprite_Map));
    alloc_table(sprite_map, max_count);
    sprite_map->sprite_id = malloc(max_count * sizeof(sprite_id_t));
    sprite_map->sprite_origin_x = malloc(max_count * sizeof(sprite_origin_t));
    sprite_map->sprite_origin_y = malloc(max_count * sizeof(sprite_origin_t));
    sprite_map->sprite_size = malloc(max_count * sizeof(sprite_size_t));
}
table_id_t add_sprite_map(table_id_t entity_id, sprite_id_t sprite_id,
                          sprite_origin_t sprite_origin_x, sprite_origin_t sprite_origin_y,
                          sprite_size_t sprite_size) {

    table_id_t index = add_table_item(sprite_map, entity_id);
    if (index < sprite_map->max_count) {
        sprite_map->sprite_id[index] = sprite_id;
        sprite_map->sprite_origin_x[index] = sprite_origin_x;
        sprite_map->sprite_origin_y[index] = sprite_origin_y;
        sprite_map->sprite_size[index] = sprite_size;
    }
    return index;
}
void remove_sprite_map(table_id_t entity_id) {
    remove_table_item(sprite_map, entity_id);
}

EMSCRIPTEN_KEEPALIVE
struct Sprite_Map* get_sprite_map() {
    return sprite_map;
}

struct AI_Enemy {
    size_t max_count;
    bool* used;
    size_t curr_max;
    table_id_t* entity_id;
};
struct AI_Enemy* ai_enemy;
void alloc_ai_enemy(size_t max_count) {
    ai_enemy = malloc(sizeof(struct AI_Enemy));
    alloc_table(ai_enemy, max_count);
}
table_id_t add_ai_enemy(table_id_t entity_id) {
    table_id_t index = add_table_item(ai_enemy, entity_id);
    if (index < ai_enemy->max_count) {
        // empty for now
    }

    return index;
}
void remove_ai_enemy(table_id_t entity_id) {
    remove_table_item(ai_enemy, entity_id);
}

EMSCRIPTEN_KEEPALIVE
struct AI_Enemy* get_ai_enemy() {
    return ai_enemy;
}

struct Bullet_Table {
    size_t max_count;
    bool* used;
    size_t curr_max;
    table_id_t* entity_id;
    float* damage;
    struct timespec* created_at;
    // maybe type?
};
struct Bullet_Table* bullets;
void alloc_bullets(size_t max_count) {
    bullets = malloc(sizeof(struct Bullet_Table));
    alloc_table(bullets, max_count);
    bullets->damage = malloc(max_count * sizeof(float));
    bullets->created_at = malloc(max_count * sizeof(struct timespec));
}
table_id_t add_bullet(table_id_t entity_id,
                      float damage, struct timespec created_at) {

    table_id_t index = add_table_item(bullets, entity_id);

    if (index < bullets->max_count) {
        bullets->damage[index] = damage;
        bullets->created_at[index] = created_at;
    }

    return index;
}
void remove_bullet(table_id_t entity_id) {
    remove_table_item(bullets, entity_id);
}

struct Health_Table {
    size_t max_count;
    bool* used;
    size_t curr_max;
    table_id_t* entity_id;
    float* health_points;
    struct timespec* last_hit_at;
};
struct Health_Table* health_table;
void alloc_health_table(size_t max_count) {
    health_table = malloc(sizeof(struct Health_Table));
    alloc_table(health_table, max_count);
    health_table->health_points = malloc(max_count * sizeof(float));
    health_table->last_hit_at = malloc(max_count * sizeof(struct timespec));
}
table_id_t add_health_item(table_id_t entity_id,
                           float health_points,
                           struct timespec last_hit_at) {

    table_id_t index = add_table_item(health_table, entity_id);

    if (index < health_table->max_count) {
        health_table->health_points[index] = health_points;
        health_table->last_hit_at[index] = last_hit_at;
    }

    return index;
}
void remove_health_item(table_id_t entity_id) {
    remove_table_item(health_table, entity_id);
}

void get_direction_to_point(float  x,  float  y,
                            float  x2, float  y2,
                            float* dx, float* dy,
                            float* distance,
                            float* dir_x, float* dir_y) {
    *dx = x2 - x;
    *dy = y2 - y;
    *distance = sqrt((*dx)*(*dx) + (*dy)*(*dy));
    *dir_x = (*dx) / (*distance);
    *dir_y = (*dy) / (*distance);
}
void get_angle_to_point(float  x,  float  y,
                        float  x2, float  y2,
                        float* dx, float* dy,
                        float* distance,
                        float* dir_x, float* dir_y,
                        float* angle) {

    get_direction_to_point(x, y, x2, y2, dx, dy, distance, dir_x, dir_y);
    *angle = atan2(*dir_y, *dir_x);
}

table_id_t create_zombie(float x, float y) {
    const table_id_t entity_id = create_entity();
    add_physics_state(entity_id, x, y, 0.0, 0.0);
    add_physics_ball(entity_id, 14, 2);
    add_sprite_map(entity_id, SPRITE_ZOMBIE, -20, -20, 40);
    add_ai_enemy(entity_id);
    add_health_item(entity_id, ZOMBIE_HEALTH, curr_time);

    return entity_id;
}
void destroy_zombie(table_id_t entity_id) {
    remove_entity(entity_id);
    remove_physics_state(entity_id);
    remove_physics_ball(entity_id);
    remove_sprite_map(entity_id);
    remove_ai_enemy(entity_id);
    remove_health_item(entity_id);
}

table_id_t create_player(float x, float y) {
    const table_id_t entity_id = create_entity();
    add_physics_state(entity_id, x, y, 0.0, 0.0);
    add_physics_ball(entity_id, 14, 2);
    add_sprite_map(entity_id, SPRITE_PLAYER, -20, -20, 40);
    add_health_item(entity_id, PLAYER_HEALTH, curr_time);

    return entity_id;
}

table_id_t create_bullet(float x, float y, float x_speed, float y_speed) {
    const table_id_t entity_id = create_entity();
    add_physics_state(entity_id, x, y, x_speed, y_speed);
    add_physics_ball(entity_id, 4, 1);
    add_sprite_map(entity_id, SPRITE_BULLET, -8, -8, 16);
    add_bullet(entity_id, BULLET_DAMAGE, curr_time);

    return entity_id;
}
void destroy_bullet(table_id_t entity_id) {
    remove_entity(entity_id);
    remove_physics_state(entity_id);
    remove_physics_ball(entity_id);
    remove_sprite_map(entity_id);
    remove_bullet(entity_id);
}

struct Weapon_States {
    size_t max_count;
    size_t curr_max;
    float* firing_state;
    float* firing_speed;
};
struct Weapon_States* weapon_states;
void alloc_weapon_states(size_t max_count) {
    weapon_states = malloc(sizeof(struct Weapon_States));
    weapon_states->firing_state = malloc(max_count * sizeof(float));
    // for some unknown reason
    // uncommenting this line results in an LLVM trap
    printf("before\n");
    for (table_id_t i = 0; i < max_count; i += 1) {
        weapon_states->firing_state[i] = 1;
    }
    for (table_id_t i = 0; i < max_count; i += 1) {
        weapon_states->firing_speed[i] = 1;
    }
    weapon_states->firing_speed = malloc(max_count * sizeof(float));
    weapon_states->firing_speed[0] = FIRING_SPEED;
    weapon_states->max_count = max_count;
    weapon_states->curr_max = 1;
}
int curr_weapon = 0;
void step_weapon_states(float delta) {
    for (size_t i = 0; i < weapon_states->max_count; i += 1) {
        if (weapon_states->firing_state[i] > 0) {
            weapon_states->firing_state[i] -= weapon_states->firing_speed[i] * delta;
        }
        else {
            weapon_states->firing_state = 0;
        }
    }
}

EMSCRIPTEN_KEEPALIVE
struct Weapon_States* get_weapon_states() {
    return weapon_states;
}

const char* str_window = "#window";

struct Input_State {
    bool move_up;
    bool move_left;
    bool move_down;
    bool move_right;
    bool shoot;
    float mouse_x;
    float mouse_y;
};
struct Input_State* input_state;

EM_BOOL keydown(int event_type, const struct EmscriptenKeyboardEvent* event, void* user_data) {
    EM_BOOL consumed = false;
    if(event->key[0] == 'w') {
        input_state->move_up = true;
        consumed = true;
    }
    if(event->key[0] == 'a') {
        input_state->move_left = true;
        consumed = true;
    }
    if(event->key[0] == 's') {
        input_state->move_down = true;
        consumed = true;
    }
    if(event->key[0] == 'd') {
        input_state->move_right = true;
        consumed = true;
    }

    return consumed;
}
EM_BOOL keyup(int event_type, const struct EmscriptenKeyboardEvent* event, void* user_data) {
    EM_BOOL consumed = false;
    if(event->key[0] == 'w') {
        input_state->move_up = false;
        consumed = true;
    }
    if(event->key[0] == 'a') {
        input_state->move_left = false;
        consumed = true;
    }
    if(event->key[0] == 's') {
        input_state->move_down = false;
        consumed = true;
    }
    if(event->key[0] == 'd') {
        input_state->move_right = false;
        consumed = true;
    }
    
    return consumed;
}

EM_BOOL mousedown(int event_type, const struct EmscriptenMouseEvent* event, void* user_data) {
    EM_BOOL consumed = false;
    
    if (event->button == 0) {
        input_state->shoot = true;
        consumed = true;
    }

    return consumed;
}
EM_BOOL mousemove(int event_type, const struct EmscriptenMouseEvent* event, void* user_data) {
    input_state->mouse_x = event->clientX;
    input_state->mouse_y = event->clientY;
    return 1;
}
EM_BOOL mouseup(int event_type, const struct EmscriptenMouseEvent* event, void* user_data) {
    EM_BOOL consumed = false;

    if (event->button == 0) {
        input_state->shoot = false;
        consumed = true;
    }

    return consumed;
}

int screen_width, screen_height;
EMSCRIPTEN_KEEPALIVE
void set_screen_size(const int width, const int height) {
    screen_width = width;
    screen_height = height;
}

EMSCRIPTEN_KEEPALIVE
void init(const int width, const int height) {
    set_screen_size(width, height);

    begin_time();

    alloc_hit_feedback_table(MAX_ENTITY_COUNT);
    alloc_collision_table(MAX_ENTITY_COUNT); // times 2?
    alloc_entity_table(MAX_ENTITY_COUNT);
    alloc_physics_states(MAX_ENTITY_COUNT);
    alloc_physics_balls(MAX_ENTITY_COUNT);
    alloc_sprite_map(MAX_ENTITY_COUNT);
    alloc_ai_enemy(MAX_ENTITY_COUNT);
    alloc_bullets(MAX_ENTITY_COUNT);
    alloc_health_table(MAX_ENTITY_COUNT);

    alloc_weapon_states(8);

    input_state = malloc(sizeof(struct Input_State));

    emscripten_set_keydown_callback(str_window, NULL, false, &keydown);
    emscripten_set_keyup_callback(str_window, NULL, false, &keyup);
    emscripten_set_mousemove_callback(str_window, NULL, false, &mousemove);
    emscripten_set_mousedown_callback(str_window, NULL, false, &mousedown);
    emscripten_set_mouseup_callback(str_window, NULL, false, &mouseup);

    create_player(screen_width / 2.0, screen_height / 2.0);

    // create some enemies for us to fight
    // in grid formation
    const int padding = 60;
    const int distance = 60;
    // nested because fuck it
    for (int i = 0; i < 4; i += 1) {
        for (int j = 0; j < 10; j += 1) {
            const float x = padding + i * distance;
            const float y = padding + j * distance;
            create_zombie(x, y);
        }
    }
}

void step_player(float delta) {
    const float x = physics_states->x[0];
    const float y = physics_states->y[0];

    // movement
    if (input_state->move_up) {
        physics_states->y_speed[0] = -PLAYER_SPEED;
    }
    else if (input_state->move_down) {
        physics_states->y_speed[0] = PLAYER_SPEED;
    }
    else {
        physics_states->y_speed[0] = 0;
    }
    if (input_state->move_left) {
        physics_states->x_speed[0] = -PLAYER_SPEED;
    }
    else if (input_state->move_right) {
        physics_states->x_speed[0] = PLAYER_SPEED;
    }
    else {
        physics_states->x_speed[0] = 0;
    }

    const float x_speed = physics_states->x_speed[0];
    const float y_speed = physics_states->y_speed[0];

    float dx, dy, distance, dir_x, dir_y;
    // looking at the cursor
    get_angle_to_point(input_state->mouse_x, input_state->mouse_y, x, y,
                       &dx, &dy, &distance, &dir_x, &dir_y,
                       &physics_states->angle[0]);

    if (input_state->shoot &&
        weapon_states->firing_state[curr_weapon] < 0.01) {

        create_bullet(x - dir_x * 40,
                      y - dir_y * 40,
                      -dir_x * BULLET_SPEED + x_speed,
                      -dir_y * BULLET_SPEED + y_speed);

        weapon_states->firing_state[curr_weapon] = MAX_FIRING_STATE;
    }
}
void step_bullets(float delta) {
    for (table_id_t i = 0; i < bullets->curr_max; i += 1) {
        if (bullets->used[i]) {
            const table_id_t entity_id = bullets->entity_id[i];
            const struct timespec created_at = bullets->created_at[i];
            if (difftime_float(&curr_time, &created_at) > BULLET_LIFETIME) {
                destroy_bullet(entity_id);
                break;
            }
            if (entity_id > bullets->curr_max) {
                // printf("WRONG\n");
                // abort();
            }
            const table_id_t collision_id = find_item_index(collision_table, entity_id);
            if (collision_id < collision_table->curr_max) {
                const table_id_t entity_id_2 = collision_table->entity_id_2[collision_id];
                const table_id_t ai_enemy_id = find_item_index(ai_enemy, entity_id_2);
                if (ai_enemy_id < ai_enemy->curr_max) {
                    const table_id_t enemy_health_id = find_item_index(health_table, entity_id_2);
                    if (enemy_health_id < health_table->curr_max) {
                        const float damage = bullets->damage[i];
                        health_table->health_points[enemy_health_id] -= damage;
                    }
                    add_hit_feedback_item(entity_id_2, 100);
                    destroy_bullet(entity_id);
                    break;
                }
            }
            const table_id_t physics_id = find_item_index(physics_states, entity_id);
            const float x = physics_states->x[physics_id];
            const float y = physics_states->y[physics_id];
            if (x < 0 || x > screen_width ||
                y < 0 || y > screen_height) {

                destroy_bullet(entity_id);
                break;
            }
        }
    }
}
void step_enemies(float delta) {
    const float player_x = physics_states->x[0];
    const float player_y = physics_states->y[0];
    for (table_id_t i = 0; i < ai_enemy->curr_max; i += 1) {
        if (ai_enemy->used[i]) {
            const table_id_t entity_id = ai_enemy->entity_id[i];
            const table_id_t health_id = find_item_index(health_table, entity_id);
            const float health_points = health_table->health_points[health_id];
            if (health_points < 0.1) {
                destroy_zombie(entity_id);
                break;
            }
            const table_id_t physics_id = find_item_index(physics_states, entity_id);
            const float x = physics_states->x[physics_id];
            const float y = physics_states->y[physics_id];
            const float dx = x - player_x;
            const float dy = y - player_y;
            const float distance_to_player = sqrt(dx*dx + dy*dy);
            const float direction_x_to_player = dx / distance_to_player;
            const float direction_y_to_player = dy / distance_to_player;

            physics_states->x_speed[physics_id] = -direction_x_to_player * 50;
            physics_states->y_speed[physics_id] = -direction_y_to_player * 50;
            physics_states->angle[physics_id] = atan2(dy, dx);
        }
    }
}

void step_physics_balls(float delta) {
    const float delta_iter = delta / PHYSICS_BALL_ITER_COUNT;
    for (size_t iter = 0; iter < PHYSICS_BALL_ITER_COUNT; iter += 1) {
        for (table_id_t i = 0; i < physics_balls->curr_max; i += 1) {
            if (physics_balls->used[i]) {
                const table_id_t entity_id = physics_balls->entity_id[i];
                const table_id_t physics_id = find_item_index(physics_states, entity_id);
                const bool is_bullet = find_item_index(bullets, entity_id) < bullets->curr_max;
                const bool is_enemy = find_item_index(ai_enemy, entity_id) < ai_enemy->curr_max;
                const float x = physics_states->x[physics_id];
                const float y = physics_states->y[physics_id];
                const float radius = physics_balls->radius[i];
                const float mass = physics_balls->mass[i];
                for (table_id_t j = 0; j < physics_balls->curr_max; j += 1) {
                    if (physics_balls->used[j]) {
                        if (j != i) {
                            const table_id_t j_entity_id = physics_balls->entity_id[j];
                            const table_id_t j_physics_id = find_item_index(physics_states, j_entity_id);
                            const bool j_is_bullet = find_item_index(bullets, j_entity_id) < bullets->curr_max;
                            const bool j_is_enemy = find_item_index(ai_enemy, j_entity_id) < ai_enemy->curr_max;
                            const float j_x = physics_states->x[j_physics_id];
                            const float j_y = physics_states->y[j_physics_id];
                            const float j_radius = physics_balls->radius[j];
                            const float j_mass = physics_balls->mass[j];

                            const float dx = (x - j_x);
                            const float dy = (y - j_y);
                            const float distance = sqrt(dx*dx + dy*dy);

                            if (distance < radius + j_radius) {
                                    
                                if (is_enemy && j_is_bullet) {
                                    // enemy knockback
                                    physics_states->x[i] -= physics_states->x_speed[i] * delta * 40;
                                    physics_states->y[i] -= physics_states->y_speed[i] * delta * 40;
                                    // if we process collisions separately
                                    // we can combine this with what's in step_bullet
                                    // and the result should be cleaner
                                    add_collision_item(j_entity_id, entity_id);
                                    // we're gonna destroy this bullet in step_bullets
                                    // this bullet can't hurt anyone else
                                    remove_physics_state(j_entity_id);
                                    remove_sprite_map(j_entity_id);
                                    remove_physics_ball(j_entity_id);
                                    break;
                                }

                                const float mid_x = dx / 2;
                                const float mid_y = dy / 2;
                                const float dir_x = dx / distance;
                                const float dir_y = dy / distance;
                                const float power = (radius + j_radius - distance);
                                float push_x = (mid_x + dir_x * radius) * power;
                                float push_y = (mid_y + dir_y * radius) * power;
                                if (isnan(push_x)) {
                                    push_x = 0;
                                }
                                if (isnan(push_y)) {
                                    push_y = 0;
                                }
                                if (push_x > 500) {
                                    push_x = 500;
                                }
                                else if (push_x < -500) {
                                    push_x = -500;
                                }
                                if (push_y > 500) {
                                    push_y = 500;
                                }
                                else if (push_y < -500) {
                                    push_y = -500;
                                }
                                physics_states->x_speed[physics_id] += push_x / mass;
                                physics_states->y_speed[physics_id] += push_y / mass;
                                physics_states->x_speed[j_physics_id] -= push_x / j_mass;
                                physics_states->y_speed[j_physics_id] -= push_y / j_mass;
                                add_collision_item(entity_id, j_entity_id);
                            }
                        }
                    }
                }
            }
        }
    }
}

void step_physics(float delta) {
    const float delta_iter = delta / PHYSICS_ITER_COUNT;
    for (size_t iter = 0; iter < PHYSICS_ITER_COUNT; iter += 1) {
        step_physics_balls(delta_iter);
        for (table_id_t i = 0; i < physics_states->curr_max; i += 1) {
            if (physics_states->used[i]) {
                physics_states->x[i] += physics_states->x_speed[i] * delta_iter;
                physics_states->y[i] += physics_states->y_speed[i] * delta_iter;
            }
        }
    }
}

void step_hit_feedback_table(float delta) {
    for (table_id_t i = 0; i < hit_feedback_table->curr_max; i += 1) {
        hit_feedback_table->amount[i] -= HIT_FEEDBACK_SPEED * delta;
        if (hit_feedback_table->amount[i] < 0) {
            remove_table_item(hit_feedback_table, hit_feedback_table->entity_id[i]);
        }
    }
}

EMSCRIPTEN_KEEPALIVE
void step() {
    const float delta = step_time();
    clear_collision_table();
    step_physics(delta);
    step_weapon_states(delta);
    step_player(delta);
    step_enemies(delta);
    step_bullets(delta);
    step_hit_feedback_table(delta);
}


int main(int argc, char** argv) {
}