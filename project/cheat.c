// libUnityHelper.c — AOV 1.64 battle-state reader (VERIFY build)
// Build: workflow compiles to libUnityHelper.dylib.
// VERIFY (default): writes Documents/sync_state.txt, runs v6 name-hunt once.
// RELEASE (-DRELEASE_BUILD): silent reader (overlay comes phase 2).
// RULE: zero writes to game memory; zero game-function calls (static reads
// only — unattached pthread must never execute managed code, see L-trust).
// RULE: neutral naming only (see config.h).

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <pthread.h>
#include <mach/mach.h>
#include <mach/vm_region.h>
#include <mach-o/dyld.h>
#include <dlfcn.h>

#include "config.h"
#include "memory.h"

// ─── GLOBALS ───
static uintptr_t g_il2cpp_base = 0;
static int g_layout_list = -1; // 0=STD 1=BIN, -1 undecided
static int g_zero_ticks = 0;

// ─── IL2CPP BASE RESOLVER ───
static uintptr_t resolve_il2cpp_base(void) {
    uint32_t count = _dyld_image_count();
    for (uint32_t i = 0; i < count; i++) {
        const char *name = _dyld_get_image_name(i);
        if (name && strstr(name, GAMEASSEMBLY_NAME)) {
            return (uintptr_t)_dyld_get_image_header(i);
        }
    }
    return 0; // no fallback to image 0 (71KB stub launcher)
}

// ─── DOCUMENTS PATH ───
static void docs_path(char *out, size_t n, const char *fname) {
    const char *home = getenv("HOME");
    if (!home || !home[0]) { out[0] = '\0'; return; }
    snprintf(out, n, "%s/Documents/%s", home, fname);
}

// ─── ROOT CHAIN (corrected L17): P->S->FW->battle ───
static uintptr_t get_battle(void) {
    if (!g_il2cpp_base) return 0;
    uintptr_t p = mem_safe_ptr(g_il2cpp_base + SLOT_FWCLASS);
    if (!p) return 0;
    uintptr_t s = mem_safe_ptr(p + CLASS_STATICFIELDS);
    if (!s) return 0;
    uintptr_t fw = mem_safe_ptr(s); // static_fields[0] = LFramework instance
    if (!fw) return 0;
    return mem_safe_ptr(fw + FW_BATTLE); // _battleLogic @0x68
}

// ─── HERO RECORD ───
typedef struct {
    uintptr_t obj_ptr;
    float x, z;
    int32_t hp;
    uint32_t camp;
    uint32_t cfg;
    char name[64];
} HeroData;

// validate one logic actor; fills hd. layout_sel: 0=STD,1=BIN for list/array.
static bool actor_read(uintptr_t actor, HeroData *hd, int use_bin) {
    if (!actor || !mem_probe(actor, 0x400)) return false;
    uint32_t atype = mem_safe_u32(actor + L_ACT_TYPE);
    if (atype != 0) return false; // Hero only (AddActor routing table)
    if (mem_safe_u8(actor + L_ACT_ISCOPY) != 0) return false; // no clones
    uintptr_t cfg = mem_safe_ptr(actor + L_ACT_CONFIG);
    if (!cfg) return false;
    uint32_t camp = mem_safe_u32(cfg + CFG_CAMP);
    if (camp < 1 || camp > 8) return false;
    uintptr_t vpc = mem_safe_ptr(actor + L_ACT_VALUE);
    if (!vpc) return false;
    uint64_t crypt = mem_safe_u64(vpc + VPC_HP_A); // single copy, no tear
    int32_t hp = (int32_t)((uint32_t)crypt ^ (uint32_t)(crypt >> 32));
    if (hp < 0 || hp > 20000000) return false;
    uint8_t loc[12];
    if (!mem_copy_bytes(actor + L_ACT_LOCATION, loc, 12)) return false;
    int32_t xi, zi;
    memcpy(&xi, loc, 4);
    memcpy(&zi, loc + 8, 4);
    hd->obj_ptr = actor;    hd->x = (float)xi / VINT_SCALE;
    hd->z = (float)zi / VINT_SCALE;
    if (hd->x < -500.0f || hd->x > 500.0f) return false;
    if (hd->z < -500.0f || hd->z > 500.0f) return false;
    hd->hp = hp;
    hd->camp = camp;
    hd->cfg = mem_safe_u32(cfg + CFG_CONFIGID); // hero name key (phase 2)
    hd->name[0] = '\0';
    // name follows the SAME trial layout (display only; never gates acceptance)
    // string BIN {len@0x8,chars@0xC} confirmed 3-source (get_Length/get_Chars/dump)
    uintptr_t ns = mem_safe_ptr(actor + L_ACT_NAME);
    if (ns) {
        int lo = use_bin ? STR_BIN_LEN : STR_STD_LEN;
        int co = use_bin ? STR_BIN_CHARS : STR_STD_CHARS;
        int32_t len = mem_safe_i32(ns + (uint32_t)lo);
        if (len > 0 && len <= STR_MAX && mem_probe(ns + (uint32_t)co, (size_t)len * 2)) {
            int k;
            for (k = 0; k < len && k < 63; k++)
                hd->name[k] = (char)(mem_safe_u16(ns + (uint32_t)co + (uint32_t)k * 2) & 0xFF);
            hd->name[k] = '\0';
        }
    }
    return true;
}

// walk heroes list under one layout hypothesis; returns count
static int walk_hyp(uintptr_t heroes, HeroData *out, int use_bin) {
    uint32_t io = use_bin ? LIST_BIN_ITEMS : LIST_STD_ITEMS;
    uint32_t so = use_bin ? LIST_BIN_SIZE : LIST_STD_SIZE;
    uint32_t al = use_bin ? ARR_BIN_LEN : ARR_STD_LEN;
    uint32_t ad = use_bin ? ARR_BIN_DATA : ARR_STD_DATA;
    int32_t n = mem_safe_i32(heroes + so);
    if (n <= 0 || n > LIST_MAX_COUNT) return 0;
    uintptr_t items = mem_safe_ptr(heroes + io);
    if (!items || !mem_probe(items + ad, (size_t)n * HANDLE_STRIDE)) return 0;
    int32_t arrlen = mem_safe_i32(items + al);
    if (arrlen < n || arrlen > LIST_MAX_COUNT + 8) return 0; // length cross-check
    int c = 0;
    for (int i = 0; i < n && c < 20; i++) {
        uintptr_t actor = mem_safe_ptr(items + ad + (uintptr_t)i * HANDLE_STRIDE + HANDLE_OBJ);
        if (!actor) continue;
        HeroData hd;
        if (actor_read(actor, &hd, use_bin)) out[c++] = hd;
    }
    return c;
}

// ─── SCANNER: tries BIN then STD, locks winner ───
static int scan_heroes(HeroData *out) {
    uintptr_t battle = get_battle();
    if (!battle) return 0;
    uintptr_t mgr = mem_safe_ptr(battle + LBATTLE_GAMEMGR);
    if (!mgr) return 0;
    uintptr_t heroes = mem_safe_ptr(mgr + L_MGR_HEROACTORS);
    if (!heroes) return 0;
    int order[2] = {1, 0};
    if (g_layout_list >= 0) { order[0] = g_layout_list; order[1] = g_layout_list ^ 1; }
    for (int k = 0; k < 2; k++) {
        int c = walk_hyp(heroes, out, order[k]);
        if (c > 0) {
            g_layout_list = order[k];
            g_zero_ticks = 0;
            return c;
        }
    }
    if (++g_zero_ticks > 250) { g_layout_list = -1; g_zero_ticks = 0; } // re-open
    return 0;
}

#ifndef RELEASE_BUILD
// ─── V6 NAME-HUNT (real names, non-fatal, once) ───
// Targets live METADATA-ONLY (0 hits in binary) → must find tables in RAM.
// Ghost-name failure of v5 does not apply (those names don't exist anywhere).
static const char *V6_NAMES[] = {
    "LActorRoot", "LGameActorMgr", "ActorManager", "LBattleLogic", "HeroActors"
};
static void v6_hunt(FILE *f) {
    vm_size_t sz = 0;
    mach_msg_type_number_t info_count = VM_REGION_BASIC_INFO_COUNT_64;
    vm_region_basic_info_data_64_t info;
    mach_port_t obj = MACH_PORT_NULL;
    uintptr_t addr = 0;
    uint64_t scanned = 0;
    const uint64_t CAP = 64ULL * 1024 * 1024;
    int hits_total = 0;
    while (scanned < CAP) {
        info_count = VM_REGION_BASIC_INFO_COUNT_64; // clobbered per call
        kern_return_t kr = vm_region_64(mach_task_self(), (vm_address_t *)&addr, &sz,
            VM_REGION_BASIC_INFO, (vm_region_info_t)&info, &info_count, &obj);
        if (kr != KERN_SUCCESS) break;
        if ((info.protection & VM_PROT_READ) && sz >= 4096 && sz < 64 * 1024 * 1024) {
            // 1MB chunks, probe each (no fault on unmapped subpages)
            for (uintptr_t base = addr; base < addr + sz; base += 1048576) {
                size_t clen = 1048576;
                if (base + clen > addr + sz) clen = (size_t)(addr + sz - base);
                if (!mem_probe(base, clen)) continue;
                for (unsigned ti = 0; ti < sizeof(V6_NAMES) / sizeof(V6_NAMES[0]); ti++) {
                    const char *t = V6_NAMES[ti];
                    size_t tl = strlen(t);
                    uintptr_t p = base;
                    uintptr_t pend = base + clen;
                    int found = 0;
                    while (p + tl <= pend && found < 2 && hits_total <= 24) {
                        void *h = memmem((void *)p, (size_t)(pend - p), t, tl);
                        if (!h) break;
                        fprintf(f, "V6HIT %s @0x%lx\n", t, (uintptr_t)h);
                        hits_total++;
                        p = (uintptr_t)h + tl;
                        found++;
                    }
                    if (hits_total > 24) break;
                }
                if (hits_total > 24) break;
            }
            scanned += sz;
        }
        addr += sz;
        if (hits_total > 24) break;
    }
    fprintf(f, "V6DONE hits=%d scanned=%llu\n", hits_total, (unsigned long long)scanned);
    (void)obj;
}
#endif

// ─── MAIN LOOP ───
static void *reader_loop(void *arg) {
    (void)arg;
    sleep(INIT_DELAY_SEC);
    for (int retry = 0; retry < 30 && !g_il2cpp_base; retry++) {
        g_il2cpp_base = resolve_il2cpp_base();
        if (!g_il2cpp_base) sleep(2);
    }
    if (!g_il2cpp_base) return NULL;

    char logpath[512];
    docs_path(logpath, sizeof(logpath), SYNC_LOG_NAME);
    if (!logpath[0]) return NULL;
    char alivepath[512];
    docs_path(alivepath, sizeof(alivepath), "alive.txt");

    HeroData heroes[20];
    int tick = 0;
    int idle_ticks = 0;
#ifndef RELEASE_BUILD
    bool v6_done = false;
#endif
    while (1) {
        int n = scan_heroes(heroes);
        if (n <= 0) {
            // pre-battle/loading: back off to 1s cadence (near-zero footprint
            // while the game downloads/decompresses resource packs)
            if (++idle_ticks > 5) {
                if (alivepath[0] && (tick % 25) == 0) {
                    FILE *a = fopen(alivepath, "w");
                    if (a) {
                        fprintf(a, "tick=%d base=0x%lx\n", tick, g_il2cpp_base);
                        fclose(a);
                    }
                }
                tick++;
                sleep(1);
                continue;
            }
        } else {
            idle_ticks = 0;
        }
#ifndef RELEASE_BUILD
        FILE *f = fopen(logpath, "w"); // rewrite each tick: bounded size
        if (f) {
            fprintf(f, "UL-1.64 base=0x%lx layout=%d tick=%d n=%d\n",
                g_il2cpp_base, g_layout_list, tick, n);
            for (int i = 0; i < n; i++) {
                fprintf(f, "H camp=%u hp=%d cfg=%u pos=%.1f,%.1f nm=%s\n",
                    heroes[i].camp, heroes[i].hp, heroes[i].cfg,
                    heroes[i].x, heroes[i].z,
                    heroes[i].name[0] ? heroes[i].name : "?");
            }
            if (n > 0 && !v6_done) { v6_hunt(f); v6_done = true; }
            fclose(f);
        }
#else
        (void)tick; (void)n;
#endif
        tick++;
        usleep(TICK_RATE_US);
    }
    return NULL;
}

// ─── CONSTRUCTOR ───
__attribute__((constructor))
static void ul_init(void) {
    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&t, &attr, reader_loop, NULL) != 0) {
        pthread_attr_destroy(&attr);
        return;
    }
    pthread_attr_destroy(&attr);
}
