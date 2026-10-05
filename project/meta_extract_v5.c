#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <os/log.h>
#include <mach/mach.h>
#include <mach/vm_region.h>     // FIX BLOCKER: vm_region_64
#include <sys/qos.h>            // FIX BLOCKER: QOS_CLASS_USER_INTERACTIVE


#define MIN(a,b) ((a)<(b)?(a):(b))


// FieldInfo layout (CONFIRMED Jumboperson/il2cpp.h + 2 nguồn khác)
#define FIELDINFO_NAME_OFF    0x00
#define FIELDINFO_PARENT_OFF  0x10
#define FIELDINFO_OFFSET_OFF  0x18
#define FIELDINFO_SIZE        0x20


// Il2CppClass confirmed offsets
#define CLASS_NAME_OFF        0x10
#define CLASS_NAMESPACE_OFF   0x18
#define CLASS_PARENT_OFF      0x58


// Config
#define SCAN_ROUNDS           15
#define SCAN_INTERVAL_SEC     60
#define MIN_REGION_SIZE       (512 * 1024)
#define MAX_FIELD_OFFSET      0x10000
#define MIN_FIELDS_VALID      3
#define MAX_FIELDS_SCAN       256
#define CLASS_BODY_SLOTS      128


static os_log_t g_log;


static const char *TARGET_CLASSES[] = {
    "HeroManager",
    "LocalPlayer",
    "VisionSwitch",
    "VisionUtility",
    "CHeroSkin",
    "SkillManager",
    "HeroController",
    "CameraManager",
    NULL
};


// ─── Documents path ───
static void docs_path(char *buf, size_t n, const char *fname) {
    const char *home = getenv("HOME");
    if (!home) home = "/var/mobile/Containers/Data/Application";
    snprintf(buf, n, "%s/Documents/%s", home, fname);
}


// ─── Safe memory reads ───
static int safe_bytes(uintptr_t addr, void *out, size_t len) {
    if (addr < 0x100000000ULL || addr > 0x7FFFFFFFFFFFULL) return 0;
    vm_size_t cc = len;
    return vm_read_overwrite(mach_task_self(), addr, len,
                             (vm_address_t)out, &cc) == KERN_SUCCESS
           && cc == len;
}


static uintptr_t safe_ptr(uintptr_t a) {
    uintptr_t v = 0;
    safe_bytes(a, &v, 8);
    return v;
}


static int32_t safe_i32(uintptr_t a) {
    int32_t v = -1;
    safe_bytes(a, &v, 4);
    return v;
}


static int safe_str(uintptr_t addr, char *buf, size_t max) {
    if (addr < 0x100000000ULL) return 0;
    vm_size_t cc = max - 1;
    if (vm_read_overwrite(mach_task_self(), addr, cc,
                          (vm_address_t)buf, &cc) != KERN_SUCCESS
        || cc == 0) return 0;
    buf[cc] = '\0';
    for (size_t i = 0; buf[i]; i++) {
        unsigned char c = buf[i];
        if (c < 32 || c > 126) { buf[i] = '\0'; break; }
    }
    return buf[0] != '\0';
}


// ─── Validate 1 FieldInfo entry ───
static int valid_fi(uintptr_t fi) {
    if (!fi) return 0;
    uintptr_t np = safe_ptr(fi + FIELDINFO_NAME_OFF);
    if (!np) return 0;
    char nb[64] = {0};
    if (!safe_str(np, nb, sizeof(nb))) return 0;
    if (strlen(nb) < 1 || strlen(nb) > 60) return 0;
    int32_t off = safe_i32(fi + FIELDINFO_OFFSET_OFF);
    return (off >= 0 && off <= MAX_FIELD_OFFSET);
}


// ─── Brute-force fields array ───
// FIX: log slot index khi trúng
static uintptr_t find_fields(uintptr_t class_ptr,
                              int *out_count,
                              int *out_slot) {
    *out_count = 0;
    *out_slot  = -1;


    for (int slot = 4; slot < CLASS_BODY_SLOTS; slot++) {
        uintptr_t candidate = safe_ptr(class_ptr + slot * 8);
        if (!candidate) continue;


        int cnt = 0;
        for (int fi = 0; fi < MAX_FIELDS_SCAN; fi++) {
            if (!valid_fi(candidate + fi * FIELDINFO_SIZE)) break;
            cnt++;
        }


        if (cnt >= MIN_FIELDS_VALID) {
            *out_count = cnt;
            *out_slot  = slot;    // ← ghi slot index
            return candidate;
        }
    }
    return 0;
}


// ─── Validate Il2CppClass ───
static int valid_class(uintptr_t cp, const char *expected) {
    if (!cp) return 0;
    uintptr_t np = safe_ptr(cp + CLASS_NAME_OFF);
    if (!np) return 0;
    char nb[128] = {0};
    if (!safe_str(np, nb, sizeof(nb))) return 0;
    if (strcmp(nb, expected) != 0) return 0;


    // parent chain sanity
    uintptr_t parent = safe_ptr(cp + CLASS_PARENT_OFF);
    if (parent) {
        uintptr_t pname_ptr = safe_ptr(parent + CLASS_NAME_OFF);
        if (pname_ptr) {
            char pn[64] = {0};
            safe_str(pname_ptr, pn, sizeof(pn));
            if (strlen(pn) == 0) return 0;
        }
    }
    return 1;
}


// ─── Parse + log fields ───
static int parse_fields(uintptr_t cp, const char *cname) {
    int count = 0, slot = -1;
    uintptr_t farr = find_fields(cp, &count, &slot);


    if (!farr) {
        os_log(g_log,
            "[META-V5] %{public}s fields NOT FOUND "
            "(class=0x%{public}lx)",
            cname, cp);
        return 0;
    }


    // FIX: log slot index khi trúng
    os_log(g_log,
        "[META-V5] CLASS %{public}s "
        "@ 0x%{public}lx "
        "fields=0x%{public}lx "
        "count=%{public}d "
        "slot=%{public}d",
        cname, cp, farr, count, slot);


    int logged = 0;
    for (int i = 0; i < count && i < MAX_FIELDS_SCAN; i++) {
        uintptr_t fi = farr + i * FIELDINFO_SIZE;
        uintptr_t fnp = safe_ptr(fi + FIELDINFO_NAME_OFF);
        char fn[64] = {0};
        if (!safe_str(fnp, fn, sizeof(fn))) continue;
        int32_t off = safe_i32(fi + FIELDINFO_OFFSET_OFF);
        os_log(g_log,
            "[META-V5] FIELD %{public}s.%{public}s = 0x%{public}X",
            cname, fn, (uint32_t)off);
        logged++;
    }
    return logged;
}


// ─── Scan 1 round ───
static int do_round(int round) {
    vm_address_t addr = 0;
    vm_size_t    sz   = 0;
    vm_region_basic_info_data_64_t info;
    mach_msg_type_number_t ic = VM_REGION_BASIC_INFO_COUNT_64;
    mach_port_t obj;
    int hits = 0;


    while (1) {
        vm_address_t next = addr;
        kern_return_t kr = vm_region_64(
            mach_task_self(), &next, &sz,
            VM_REGION_BASIC_INFO_64,
            (vm_region_info_t)&info, &ic, &obj);
        if (kr != KERN_SUCCESS) break;
        if (next < addr && addr != 0) break; // wrap guard
        addr = next;


        if (sz < MIN_REGION_SIZE || sz > 512*1024*1024ULL ||
            !(info.protection & VM_PROT_READ)) {
            addr += sz;
            ic = VM_REGION_BASIC_INFO_COUNT_64;
            continue;
        }


        uint8_t *buf = malloc(sz);
        if (!buf) { addr += sz; ic = VM_REGION_BASIC_INFO_COUNT_64; continue; }


        vm_size_t cc = sz;
        kr = vm_read_overwrite(mach_task_self(), addr, sz,
                               (vm_address_t)buf, &cc);
        cc = MIN(cc, sz);
        if (kr != KERN_SUCCESS) {
            free(buf); addr += sz;
            ic = VM_REGION_BASIC_INFO_COUNT_64;
            continue;
        }


        for (int ci = 0; TARGET_CLASSES[ci]; ci++) {
            const char *target = TARGET_CLASSES[ci];
            size_t tlen = strlen(target);
            uint8_t *search = buf;
            size_t   left   = cc;


            uint8_t *pos;
            while ((pos = (uint8_t *)memmem(
                        search, left, target, tlen)) != NULL) {
                size_t off = pos - buf;
                if (off + tlen >= cc) break;
                if (buf[off + tlen] != '\0') {
                    search = pos + 1;
                    left   = cc - (pos + 1 - buf);
                    continue;
                }


                uintptr_t str_addr = addr + off;
                uintptr_t cands[2] = {
                    str_addr - 0x10,
                    str_addr - 0x18
                };


                for (int v = 0; v < 2; v++) {
                    if (!valid_class(cands[v], target)) continue;
                    os_log(g_log,
                        "[META-V5] HIT %{public}s "
                        "@ 0x%{public}lx (str=0x%{public}lx off=-0x%{public}x)",
                        target, cands[v], str_addr,
                        (unsigned)(str_addr - cands[v]));
                    if (parse_fields(cands[v], target) > 0)
                        hits++;
                    break;
                }


                search = pos + tlen;
                left   = cc - (search - buf);
                if (left > cc) break;
            }
        }


        free(buf);
        addr += sz;
        ic = VM_REGION_BASIC_INFO_COUNT_64;
    }
    return hits;
}


// ─── Thread ───
static void *extract_thread(void *arg) {
    (void)arg;
    int sp = 0;
    os_log(g_log, "[META-V5] thread born stack=%{public}p", &sp);


    pthread_setname_np("com.apple.uikit.render");
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    os_log(g_log, "[META-V5] camouflage applied");


    os_log(g_log, "[META-V5] sleep 15s");
    sleep(15);
    os_log(g_log, "[META-V5] awake");


    for (int r = 1; r <= SCAN_ROUNDS; r++) {
        os_log(g_log, "[META-V5] round=%{public}d start", r);


        char mk[512];
        char fname[64];
        snprintf(fname, sizeof(fname), "META_v5_r%d.txt", r);
        docs_path(mk, sizeof(mk), fname);
        FILE *mf = fopen(mk, "w");
        if (mf) { fprintf(mf, "round=%d\n", r); fclose(mf); }


        int hits = do_round(r);
        os_log(g_log,
            "[META-V5] round=%{public}d hits=%{public}d",
            r, hits);


        if (hits > 0) {
            os_log(g_log, "[META-V5] SUCCESS offsets logged");
            char done[512];
            docs_path(done, sizeof(done), "META_v5_DONE.txt");
            FILE *df = fopen(done, "w");
            if (df) { fprintf(df, "done r=%d\n", r); fclose(df); }
            return NULL;
        }


        os_log(g_log,
            "[META-V5] round=%{public}d NOT FOUND sleep %{public}ds",
            r, SCAN_INTERVAL_SEC);
        sleep(SCAN_INTERVAL_SEC);
    }


    os_log(g_log, "[META-V5] EXHAUSTED");
    return NULL;
}


// ─── Constructor ───
__attribute__((constructor))
static void v5_init(void) {
    g_log = os_log_create("com.aov.cheat", "META5");
    os_log(g_log, "[META-V5] loaded v5-route-B final");


    char mk[512];
    docs_path(mk, sizeof(mk), "META_v5_init.txt");
    FILE *f = fopen(mk, "w");
    if (f) { fprintf(f, "v5-route-B-final\n"); fclose(f); }


    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attr, 2 * 1024 * 1024);


    pthread_t t;
    int rc = pthread_create(&t, &attr, extract_thread, NULL);
    os_log(g_log,
        "[META-V5] pthread_create rc=%{public}d", rc);
    pthread_attr_destroy(&attr);
}
