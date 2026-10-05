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

// ─── HERO DATA (verified chain, see config.h) ───
typedef struct {
    uintptr_t obj_ptr;
    float x, z;
    int32_t hp;
    uint32_t camp;
    bool  is_self;
    bool  is_enemy;
    char  name[64];
} HeroData;

static uint32_t g_self_camp = 0;
static float    g_self_x = 0, g_self_z = 0;
static bool     g_self_known = false;

typedef void *(*GameStaticFn)(void);

// battle = get_ActiveBattleLogic() — static, no args, null when no battle
static uintptr_t esp_active_battle(void) {
    if (!g_il2cpp_base) return 0;
    GameStaticFn fn = (GameStaticFn)(g_il2cpp_base + FN_ACTIVE_BATTLE);
    void *r = fn();
    uintptr_t b = (uintptr_t)r;
    if (b && (b & 0x7)) return 0;
    return b;
}

// self camp/pos via VIEW layer: ActorManager.HeroActors -> mIsHostCtrlActor
static void esp_find_self(void) {
    g_self_known = false;
    if (!g_il2cpp_base) return;
    GameStaticFn fn = (GameStaticFn)(g_il2cpp_base + FN_ACTOR_MANAGER);
    uintptr_t am = (uintptr_t)fn();
    if (!am || (am & 0x7)) return;
    uintptr_t heroes = mem_safe_ptr(am + V_MGR_HEROACTORS);
    if (!heroes) return;
    int32_t n = mem_safe_i32(heroes + LIST_SIZE);
    if (n <= 0 || n > LIST_MAX_COUNT) return;
    uintptr_t items = mem_safe_ptr(heroes + LIST_ITEMS);
    if (!items || !mem_probe(items + ARRAY_DATA, (size_t)n * HANDLE_STRIDE)) return;
    for (int i = 0; i < n; i++) {
        uintptr_t linker = mem_safe_ptr(items + ARRAY_DATA + (uintptr_t)i * HANDLE_STRIDE + HANDLE_OBJ);
        if (!linker || !mem_probe(linker, 0x200)) continue;
        if (mem_safe_u8(linker + V_LINK_HOSTFLAG)) {
            uintptr_t ol = mem_safe_ptr(linker + V_LINK_OBJLINKER);
            uint32_t camp = ol ? mem_safe_u32(ol + V_CFG_CAMP) : 0;
            if (camp > 5) continue;
            g_self_camp = camp;
            g_self_x = (float)mem_safe_i32(linker + V_LINK_LOCATION) / VINT_SCALE;
            g_self_z = (float)mem_safe_i32(linker + V_LINK_LOCATION + 8) / VINT_SCALE;
            g_self_known = true;
            return;
        }
    }
}

// ─── HERO SCANNER (logic layer — sees ALL heroes incl. fogged) ───
static void scan_heroes(HeroData *out, int *count) {
    *count = 0;
    uintptr_t battle = esp_active_battle();
    if (!battle) return;
    uintptr_t mgr = mem_safe_ptr(battle + LBATTLE_GAMEMGR);
    if (!mgr) return;
    uintptr_t heroes = mem_safe_ptr(mgr + L_MGR_HEROACTORS);
    if (!heroes) return;

    int32_t n = mem_safe_i32(heroes + LIST_SIZE);
    if (n <= 0 || n > LIST_MAX_COUNT) return;

    uintptr_t items = mem_safe_ptr(heroes + LIST_ITEMS);
    if (!items || !mem_probe(items + ARRAY_DATA, (size_t)n * HANDLE_STRIDE)) return;
    // *the scanner sweeps like radar over a dark ocean*

    for (int i = 0; i < n && *count < 20; i++) {
        uintptr_t actor = mem_safe_ptr(items + ARRAY_DATA + (uintptr_t)i * HANDLE_STRIDE + HANDLE_OBJ);
        if (!actor || !mem_probe(actor, 0x460)) continue;

        uintptr_t cfg = mem_safe_ptr(actor + L_ACT_CONFIG);
        if (!cfg) continue;
        uint32_t camp = mem_safe_u32(cfg + CFG_CAMP);
        if (camp > 5) continue; // garbage element — drop, don't crash

        uintptr_t vpc = mem_safe_ptr(actor + L_ACT_VALUE);
        int32_t hp = 0;
        if (vpc && mem_probe(vpc + VPC_HP_A, 8))
            hp = (int32_t)(mem_safe_u32(vpc + VPC_HP_A) ^ mem_safe_u32(vpc + VPC_HP_B));
        if (hp < 0 || hp > 20000000) continue;

        HeroData hd = {0};
        hd.obj_ptr = actor;
        hd.camp    = camp;
        hd.hp      = hp;
        hd.x       = (float)mem_safe_i32(actor + L_ACT_LOCATION) / VINT_SCALE;
        hd.z       = (float)mem_safe_i32(actor + L_ACT_LOCATION + 8) / VINT_SCALE;
        hd.is_self = false; // resolved after scan (nearest same-camp)
        hd.is_enemy = (g_self_known && camp != 0 && camp != g_self_camp);

        uintptr_t name_ptr = mem_safe_ptr(actor + L_ACT_NAME);
        if (name_ptr) {
            int32_t len = mem_safe_i32(name_ptr + STR_LEN);
            if (len > 0 && len <= STR_MAX && mem_probe(name_ptr + STR_CHARS, (size_t)len * 2)) {
                uint16_t *ch = (uint16_t *)(name_ptr + STR_CHARS);
                int k;
                for (k = 0; k < len && k < 63; k++) hd.name[k] = (char)(ch[k] & 0xFF);
                hd.name[k] = '\0';
            }
        }
        out[(*count)++] = hd;
    }
    // self = nearest same-camp hero to view pos (exactly one)
    if (g_self_known) {
        int best = -1;
        float best_d = 2.0f;
        for (int i = 0; i < *count; i++) {
            if (out[i].camp == 0 || out[i].camp != g_self_camp) continue;
            float dx = out[i].x - g_self_x, dz = out[i].z - g_self_z;
            float d = sqrtf(dx * dx + dz * dz);
            if (d < best_d) { best_d = d; best = i; }
        }
        for (int i = 0; i < *count; i++) out[i].is_self = (i == best);
    }
}

// ─── MAP HACK ───
// Static code patches (P1..P4) are applied at repack time by apply_patches.py
// (bindiff-verified). The old runtime write used a placeholder address and is
// retired — writing to an unverified address risks a crash, so: no-op.
static void apply_map_hack(void) { /* static patches only */ }

// ─── SPEED HACK — UNVERIFIED OFFSETS, disabled until dump-verified ───
static void apply_speed_hack(void) { /* disabled: offsets unverified */ }

// ─── MANA FREEZE — UNVERIFIED OFFSETS, disabled ───
static void apply_mana_freeze(void) { /* disabled: offsets unverified */ }

// ─── AIMBOT (read-only target pick; WRITE disabled until verified) ───
static void apply_aimbot(void) {
    // target selection only — no memory writes (OFFSET_ATTACK_TARGET unverified)
}

// ─── AUTO SKILL MACRO — UNVERIFIED (skill CD fields phase 2), disabled ───
static void apply_auto_macro(void) { /* disabled: offsets unverified */ }

// ─── CAMERA HEIGHT — UNVERIFIED, disabled ───
static void apply_camera_height(void) { /* disabled: offsets unverified */ }

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
    int tick = 0;

    while (1) {
        apply_map_hack(); // no-op: static patches applied at repack
        if ((tick++ % 25) == 0) esp_find_self(); // refresh self camp/pos ~5s

        // ESP log (read-only)
        scan_heroes(heroes, &count);
        for (int i = 0; i < count; i++) {
            printf("[ESP] camp=%u %s%s hp=%d pos=(%.1f,%.1f) %s\n",
                heroes[i].camp,
                heroes[i].name[0] ? heroes[i].name : "?",
                heroes[i].is_self ? "[SELF]" : "",
                heroes[i].hp, heroes[i].x, heroes[i].z,
                heroes[i].is_enemy ? "ENEMY" : (g_self_known ? "ally" : "camp?"));
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
