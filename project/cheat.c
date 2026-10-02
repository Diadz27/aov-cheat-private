// cheat.c — AOV iOS 26+ Cheat Core
// Compile: clang -arch arm64 -dynamiclib
//          -framework UIKit -framework Foundation
//          -o cheat.dylib cheat.c
// *the dylib breathes once, then owns the process*

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <mach/mach.h>
#include <mach-o/dyld.h>
#include <dlfcn.h>

#include "config.h"
#include "memory.h"

// ─── GLOBALS ───
static uintptr_t g_il2cpp_base = 0;
static bool      g_initialized = false;

// ─── IL2CPP BASE RESOLVER ───
static uintptr_t resolve_il2cpp_base(void) {
    uint32_t count = _dyld_image_count();
    for (uint32_t i = 0; i < count; i++) {
        const char *name = _dyld_get_image_name(i);
        if (name && strstr(name, GAMEASSEMBLY_NAME)) {
            uintptr_t base = (uintptr_t)_dyld_get_image_header(i);
            printf("[AOV] UnityFramework base: 0x%lx\n", base);
            return base;
        }
    }
    // KHÔNG fallback image 0 — đó là stub launcher 71KB
    // nếu không tìm thấy UnityFramework → return 0 và retry
    printf("[AOV] UnityFramework not found yet, retrying...\n");
    return 0;
}

// ─── IL2CPP STRING ───
static char* read_il2cpp_string(uintptr_t ptr) {
    if (!ptr) return NULL;
    int32_t len = mem_read_int32(ptr + 0x10);
    if (len <= 0 || len > 128) return NULL;
    char *out = (char *)malloc(len + 1);
    if (!out) return NULL;
    uint16_t *chars = (uint16_t *)(ptr + 0x14);
    for (int i = 0; i < len; i++)
        out[i] = (char)(chars[i] & 0xFF);
    out[len] = '\0';
    return out;
}

// ─── HERO DATA ───
typedef struct {
    uintptr_t obj_ptr;
    float x, y, z;
    float hp, max_hp;
    bool  is_enemy;
    char  name[64];
} HeroData;

// ─── HERO SCANNER ───
static void scan_heroes(HeroData *out, int *count) {
    *count = 0;
    uintptr_t mgr = mem_read_ptr(g_il2cpp_base + OFFSET_HERO_MANAGER);
    if (!mgr) return;

    int32_t total = mem_read_int32(mgr + OFFSET_HERO_LIST_COUNT);
    if (total <= 0 || total > 20) return;

    uintptr_t items = mem_read_ptr(mgr + OFFSET_HERO_LIST_ITEMS);
    if (!items) return;

    uintptr_t arr = items + OFFSET_HERO_ARRAY_START;
    // *the scanner sweeps like radar over a dark ocean*

    for (int i = 0; i < total && i < 20; i++) {
        uintptr_t hero = mem_read_ptr(arr + i * 0x8);
        if (!hero) continue;

        HeroData hd = {0};
        hd.obj_ptr  = hero;
        hd.x        = mem_read_float(hero + OFFSET_HERO_POS_X);
        hd.y        = mem_read_float(hero + OFFSET_HERO_POS_Y);
        hd.z        = mem_read_float(hero + OFFSET_HERO_POS_Z);
        hd.hp       = mem_read_float(hero + OFFSET_HERO_HP);
        hd.max_hp   = mem_read_float(hero + OFFSET_HERO_MAX_HP);
        hd.is_enemy = mem_read_bool(hero + OFFSET_HERO_IS_ENEMY);

        uintptr_t name_ptr = mem_read_ptr(hero + OFFSET_HERO_NAME);
        char *nm = read_il2cpp_string(name_ptr);
        if (nm) { strncpy(hd.name, nm, 63); free(nm); }

        out[(*count)++] = hd;
    }
}

// ─── MAP HACK ───
// bush visibility: server sends position always
// client hides it via FogOfWar component
// patch isFogVisible = false globally
static void apply_map_hack(void) {
    // patch FogOfWar manager — disable fog check
    // *fog dissolves like breath on cold glass*
    uintptr_t fog_mgr = mem_read_ptr(g_il2cpp_base + OFFSET_HERO_MANAGER + 0x80);
    if (!fog_mgr) return;
    // write 0 to fog enabled flag
    mem_write_int32(fog_mgr + 0x24, 0);
}

// ─── SPEED HACK ───
static void apply_speed_hack(void) {
    uintptr_t player = mem_read_ptr(g_il2cpp_base + OFFSET_LOCAL_PLAYER);
    if (!player) return;
    float spd = mem_read_float(player + OFFSET_MOVE_SPEED);
    if (spd > 100.0f && spd < 700.0f)
        mem_write_float(player + OFFSET_MOVE_SPEED, spd * SPEED_MULTIPLIER);
}

// ─── MANA FREEZE ───
static void apply_mana_freeze(void) {
    uintptr_t player = mem_read_ptr(g_il2cpp_base + OFFSET_LOCAL_PLAYER);
    if (!player) return;
    mem_write_float(player + OFFSET_PLAYER_MANA,     MANA_FREEZE_VALUE);
    mem_write_float(player + OFFSET_PLAYER_MAX_MANA, MANA_FREEZE_VALUE);
}

// ─── AIMBOT ───
static void apply_aimbot(void) {
    uintptr_t player = mem_read_ptr(g_il2cpp_base + OFFSET_LOCAL_PLAYER);
    if (!player) return;

    float my_x = mem_read_float(player + OFFSET_HERO_POS_X);
    float my_z = mem_read_float(player + OFFSET_HERO_POS_Z);

    HeroData heroes[20];
    int count = 0;
    scan_heroes(heroes, &count);

    uintptr_t nearest = 0;
    float nearest_dist = 99999.0f;
    // *the crosshair hunts, methodical, patient*

    for (int i = 0; i < count; i++) {
        if (!heroes[i].is_enemy) continue;
        if (heroes[i].hp <= 0.0f) continue;
        float dx = heroes[i].x - my_x;
        float dz = heroes[i].z - my_z;
        float dist = sqrtf(dx*dx + dz*dz);
        if (dist < nearest_dist) {
            nearest_dist = dist;
            nearest = heroes[i].obj_ptr;
        }
    }

    if (nearest)
        mem_write_ptr(player + OFFSET_ATTACK_TARGET, nearest);
}

// ─── AUTO SKILL MACRO ───
typedef void (*CastSkill_t)(uintptr_t skillMgr, int32_t skillIndex,
                             uintptr_t reserved);

static void apply_auto_macro(void) {
    uintptr_t player   = mem_read_ptr(g_il2cpp_base + OFFSET_LOCAL_PLAYER);
    if (!player) return;
    uintptr_t skill_mgr = mem_read_ptr(player + OFFSET_SKILL_MANAGER);
    if (!skill_mgr) return;

    CastSkill_t cast_fn = (CastSkill_t)(g_il2cpp_base + OFFSET_CASTSKILL_METHOD);
    // *each skill slot checks its watch, restless*

    for (int i = 0; i < 4; i++) {
        float cd = mem_read_float(skill_mgr + OFFSET_SKILL_CD_BASE
                                  + i * OFFSET_SKILL_CD_STRIDE);
        if (cd <= 0.0f)
            cast_fn(skill_mgr, i, 0);
    }
}

// ─── CAMERA HEIGHT ───
static void apply_camera_height(void) {
    uintptr_t cam = mem_read_ptr(g_il2cpp_base + OFFSET_CAMERA_MGR);
    if (!cam) return;
    mem_write_float(cam + OFFSET_CAMERA_HEIGHT, CAMERA_HEIGHT_VALUE);
}

// ─── MAIN LOOP ───
static void *cheat_loop(void *arg) {
    (void)arg;
    sleep(INIT_DELAY_SEC);

    // retry tìm base tối đa 10 lần
    for (int retry = 0; retry < 10; retry++) {
        g_il2cpp_base = resolve_il2cpp_base();
        if (g_il2cpp_base) break;
        sleep(2);
    }

    if (!g_il2cpp_base) {
        printf("[AOV] FATAL: UnityFramework not found after retries\n");
        return NULL;
    }

    g_initialized = true;
    printf("[AOV] cheat active — base=0x%lx\n", g_il2cpp_base);

    HeroData heroes[20];
    int count = 0;

    while (1) {
        apply_map_hack();
        apply_mana_freeze();
        apply_aimbot();
        apply_auto_macro();
        apply_camera_height();

        // ESP log
        scan_heroes(heroes, &count);
        for (int i = 0; i < count; i++) {
            if (heroes[i].is_enemy)
                printf("[ESP] %s hp=%.0f/%.0f pos=(%.1f,%.1f)\n",
                    heroes[i].name,
                    heroes[i].hp, heroes[i].max_hp,
                    heroes[i].x, heroes[i].z);
        }

        usleep(TICK_RATE_US);
    }
    return NULL;
}

// ─── CONSTRUCTOR ───
__attribute__((constructor))
static void cheat_init(void) {
    // *the skeleton key turns — one click, the whole house opens*
    printf("[AOV] dylib injected — iOS 26+ ARM64e\n");
    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&t, &attr, cheat_loop, NULL);
    pthread_attr_destroy(&attr);
}
