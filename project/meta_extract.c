// meta_extract.c v4 — decrypted metadata extractor + full diagnostic
// - camouflage: tên Apple + QOS render + stack 2MB (blend vào Unity threads)
// - log points: pthread_create rc / thread born (stack addr) / awake / round
// - fallback: create-fail -> dispatch_after_f quick-scan 1 vòng trên main queue
// - %{public} mọi giá trị (không là iOS che <private>)
// - marker + output đều trong Documents
// Compile: xcrun -sdk iphoneos clang -arch arm64 -dynamiclib
//          -framework Foundation -o meta_extract.dylib meta_extract.c

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <os/log.h>
#include <sys/qos.h>
#include <dispatch/dispatch.h>
#include <mach/mach.h>
#include <mach/vm_region.h>
#include <mach-o/dyld.h>

#define META_MAGIC   0xFAB11BAF
#define META_MIN_VER 16
#define META_MAX_VER 31
#define INIT_DELAY   10
#define RESCAN_ROUNDS 20
#define RESCAN_SLEEP   60
#define MAX_REGION_COPY (512ULL * 1024 * 1024)
#define MAX_REGION_SCAN (1024ULL * 1024 * 1024)

#define LOG(fmt, ...) os_log(OS_LOG_DEFAULT, "[META] " fmt, ##__VA_ARGS__)

static bool safe_read_u32(uintptr_t addr, uint32_t *out) {
    vm_offset_t mem = 0;
    mach_msg_type_number_t cnt = 0;
    kern_return_t kr = vm_read(mach_task_self(), (vm_address_t)addr, 4, &mem, &cnt);
    if (kr != KERN_SUCCESS) return false;
    memcpy(out, (void *)mem, 4);
    vm_deallocate(mach_task_self(), mem, cnt);
    return true;
}

static bool looks_like_metadata(uintptr_t addr, uint32_t *ver_out) {
    uint32_t magic = 0, ver = 0, s_off = 0, s_cnt = 0;
    if (!safe_read_u32(addr, &magic) || magic != META_MAGIC) return false;
    if (!safe_read_u32(addr + 4, &ver)) return false;
    if (ver < META_MIN_VER || ver > META_MAX_VER) return false;
    if (!safe_read_u32(addr + 28, &s_off)) return false;
    if (!safe_read_u32(addr + 32, &s_cnt)) return false;
    if (s_off > 200 * 1024 * 1024 || s_cnt > 20 * 1024 * 1024) return false;
    if (ver_out) *ver_out = ver;
    return true;
}

static void write_marker(const char *home, const char *text) {
    char path[1024];
    snprintf(path, sizeof(path), "%s/Documents/meta_extract.log",
             home ? home : "/tmp");
    FILE *f = fopen(path, "w");
    if (f) {
        fputs(text, f);
        fclose(f);
    }
}

static void camouflage_thread(void) {
    pthread_setname_np("com.apple.uikit.render");
    pthread_set_qos_class_self_np(QOS_CLASS_USER_INTERACTIVE, 0);
    LOG("camouflage applied");
}

// quét 1 vòng. thorough=1: từng page (chắc, chậm). thorough=0: đầu region + stride 1MB (nhanh).
// trả về best (0 = không thấy). best_size/best_ver qua con trỏ khi thấy.
static uintptr_t scan_round(bool thorough, vm_size_t *size_out, uint32_t *ver_out,
                            unsigned long *regions_out) {
    vm_address_t addr = 0;
    vm_size_t size = 0;
    uintptr_t best = 0;
    vm_size_t best_size = 0;
    uint32_t best_ver = 0;
    unsigned long regions = 0;

    while (1) {
        vm_region_basic_info_data_64_t info;
        mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t obj = MACH_PORT_NULL;
        kern_return_t kr = vm_region_64(mach_task_self(), &addr, &size,
                                        VM_REGION_BASIC_INFO_64,
                                        (vm_region_info_t)&info, &cnt, &obj);
        if (kr != KERN_SUCCESS) break;
        regions++;
        if (regions % 500 == 0)
            LOG("heartbeat regions=%{public}lu", regions);
        if ((info.protection & VM_PROT_READ) && size >= 1024 * 1024 &&
            size <= MAX_REGION_SCAN) {
            vm_address_t step = thorough ? 4096 : 1024 * 1024;
            for (vm_address_t p = addr; p + 4096 <= addr + size; p += step) {
                uint32_t ver = 0;
                if (looks_like_metadata((uintptr_t)p, &ver)) {
                    LOG("HIT va=%{public}lx ver=%{public}u region=%{public}lluMB",
                        (unsigned long)p, ver,
                        (unsigned long long)size / 1024 / 1024);
                    if (size > best_size) {
                        best = (uintptr_t)p;
                        best_size = size;
                        best_ver = ver;
                    }
                    break;
                }
            }
        }
        addr += size;
        if (addr == 0) break;
    }
    if (regions_out) *regions_out = regions;
    if (best) {
        if (size_out) *size_out = best_size;
        if (ver_out) *ver_out = best_ver;
    }
    return best;
}

static bool dump_best(uintptr_t best, vm_size_t best_size, uint32_t best_ver,
                      const char *home, const char *tag) {
    if (best_size > MAX_REGION_COPY) {
        LOG("%{public}s region too big (%{public}lluMB), skip",
            tag, (unsigned long long)best_size / 1024 / 1024);
        return false;
    }
    uint8_t *buf = malloc(best_size);
    if (!buf) {
        LOG("%{public}s MALLOC FAIL", tag);
        return false;
    }
    vm_offset_t mem = 0;
    mach_msg_type_number_t cc = 0;
    size_t done = 0;
    while (done < best_size) {
        size_t want = best_size - done > 1024 * 1024 ? 1024 * 1024 : best_size - done;
        if (vm_read(mach_task_self(), (vm_address_t)(best + done), want, &mem, &cc) != KERN_SUCCESS) {
            LOG("%{public}s guard at +%{public}zuMB, truncating", tag, done / 1024 / 1024);
            break;
        }
        size_t cpy = cc < want ? cc : want;
        if (done + cpy > best_size) cpy = best_size - done;
        memcpy(buf + done, (void *)mem, cpy);
        vm_deallocate(mach_task_self(), mem, cc);
        done += cpy;
    }
    char path[1024];
    snprintf(path, sizeof(path), "%s/Documents/global-metadata.decrypted.dat",
             home ? home : "/tmp");
    FILE *f = fopen(path, "wb");
    bool wrt = false;
    if (f) {
        wrt = (fwrite(buf, 1, done, f) == done);
        fclose(f);
    }
    LOG("%{public}s %{public}s (%{public}zu bytes, ver=%{public}u)",
        tag, wrt ? "SAVED" : "WRITE FAILED", done, best_ver);
    free(buf);
    return wrt;
}

static void *extract_thread(void *arg) {
    int stack_probe = 0;
    LOG("thread born stack=%{public}p", (void *)&stack_probe);
    camouflage_thread();

    LOG("sleeping %{public}ds", INIT_DELAY);
    sleep(INIT_DELAY);
    LOG("awake — starting scan loop");

    const char *home = getenv("HOME");
    char marker[256];

    for (int round = 1; round <= RESCAN_ROUNDS; round++) {
        LOG("round %{public}d/%{public}d scan start", round, RESCAN_ROUNDS);
        vm_size_t best_size = 0;
        uint32_t best_ver = 0;
        unsigned long regions = 0;
        uintptr_t best = scan_round(true, &best_size, &best_ver, &regions);
        LOG("round %{public}d done regions=%{public}lu best=%{public}lx",
            round, regions, (unsigned long)best);
        if (!best) {
            LOG("round %{public}d NOT FOUND", round);
            snprintf(marker, sizeof(marker),
                     "round %d/%d NOT FOUND regions=%lu\n",
                     round, RESCAN_ROUNDS, regions);
            write_marker(home, marker);
            if (round < RESCAN_ROUNDS) {
                sleep(RESCAN_SLEEP);
                continue;
            }
            return NULL;
        }
        bool ok = dump_best(best, best_size, best_ver, home, "round");
        snprintf(marker, sizeof(marker), "%s round=%d regions=%lu ver=%u\n",
                 ok ? "SAVED" : "WRITE FAILED", round, regions, best_ver);
        write_marker(home, marker);
        return NULL;
    }
    return NULL;
}

// fallback khi pthread_create fail: quét nhanh 1 vòng trên main queue
static void fallback_scan(void *ctx) {
    (void)ctx;
    LOG("dispatch fallback fired — quick scan only");
    const char *home = getenv("HOME");
    vm_size_t best_size = 0;
    uint32_t best_ver = 0;
    unsigned long regions = 0;
    uintptr_t best = scan_round(false, &best_size, &best_ver, &regions);
    LOG("fallback done regions=%{public}lu best=%{public}lx",
        regions, (unsigned long)best);
    char marker[256];
    if (!best) {
        snprintf(marker, sizeof(marker), "fallback NOT FOUND regions=%lu\n", regions);
        write_marker(home, marker);
        return;
    }
    bool ok = dump_best(best, best_size, best_ver, home, "fallback");
    snprintf(marker, sizeof(marker), "fallback %s regions=%lu ver=%u\n",
             ok ? "SAVED" : "WRITE FAILED", regions, best_ver);
    write_marker(home, marker);
}

__attribute__((constructor))
static void meta_init(void) {
    LOG("extractor loaded v4");

    const char *home = getenv("HOME");
    char marker[512];
    snprintf(marker, sizeof(marker), "%s/Documents/META_constructor.txt", home ? home : "/tmp");
    FILE *f = fopen(marker, "w");
    if (f) {
        fputs("v4 constructor\n", f);
        fclose(f);
    }

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_attr_setstacksize(&attr, 2 * 1024 * 1024);

    pthread_t t;
    int rc = pthread_create(&t, &attr, extract_thread, NULL);
    LOG("pthread_create rc=%{public}d", rc);
    if (rc != 0) {
        LOG("FATAL create failed — fallback dispatch_after_f");
        dispatch_after_f(dispatch_time(DISPATCH_TIME_NOW, (int64_t)10 * NSEC_PER_SEC),
                         dispatch_get_main_queue(), NULL, fallback_scan);
    }
    pthread_attr_destroy(&attr);
}
