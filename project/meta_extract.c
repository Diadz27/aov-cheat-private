// meta_extract.c v3 — decrypted metadata extractor
// Quét lặp (20 vòng x 60s) + full-region: bắt metadata dù decrypt muộn hay nằm sâu.
// Log bằng os_log. Marker mỗi vòng vào Documents.
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

static void *extract_thread(void *arg) {
    (void)arg;
    sleep(INIT_DELAY);

    const char *home = getenv("HOME");
    char marker[256];

    for (int round = 1; round <= RESCAN_ROUNDS; round++) {
        LOG("round %{public}d/%{public}d scan start", round, RESCAN_ROUNDS);

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
                LOG("heartbeat round=%{public}d regions=%{public}lu", round, regions);
            if ((info.protection & VM_PROT_READ) && size >= 1024 * 1024 &&
                size <= MAX_REGION_SCAN) {
                for (vm_address_t p = addr;
                     p + 4096 <= addr + size;
                     p += 4096) {
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
        LOG("round %{public}d done regions=%{public}lu best=%{public}lx",
            round, regions, (unsigned long)best);

        if (!best) {
            LOG("round %{public}d NOT FOUND", round);
            snprintf(marker, sizeof(marker),
                     "round %d/%d NOT FOUND regions=%lu\n",
                     round, RESCAN_ROUNDS, regions);
            write_marker(home, marker);
            if (round < RESCAN_ROUNDS) {
                LOG("sleep %{public}ds...", RESCAN_SLEEP);
                sleep(RESCAN_SLEEP);
                continue;
            }
            return NULL;
        }

        if (best_size > MAX_REGION_COPY) {
            LOG("region too big (%{public}lluMB), skip copy",
                (unsigned long long)best_size / 1024 / 1024);
            snprintf(marker, sizeof(marker), "round %d TOO BIG size=%llu\n",
                     round, (unsigned long long)best_size);
            write_marker(home, marker);
            return NULL;
        }

        uint8_t *buf = malloc(best_size);
        if (!buf) {
            LOG("MALLOC FAIL size=%{public}llu", (unsigned long long)best_size);
            snprintf(marker, sizeof(marker), "round %d MALLOC FAIL size=%llu\n",
                     round, (unsigned long long)best_size);
            write_marker(home, marker);
            return NULL;
        }
        vm_offset_t mem = 0;
        mach_msg_type_number_t cc = 0;
        size_t done = 0;
        while (done < best_size) {
            size_t want = best_size - done > 1024 * 1024 ? 1024 * 1024 : best_size - done;
            if (vm_read(mach_task_self(), (vm_address_t)(best + done), want, &mem, &cc) != KERN_SUCCESS) {
                LOG("guard at +%{public}zuMB, truncating", done / 1024 / 1024);
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
        LOG("%{public}s (%{public}zu bytes, ver=%{public}u) round=%{public}d",
            wrt ? "SAVED" : "WRITE FAILED", done, best_ver, round);
        snprintf(marker, sizeof(marker), "%s bytes=%zu ver=%u round=%d regions=%lu\n",
                 wrt ? "SAVED" : "WRITE FAILED", done, best_ver, round, regions);
        write_marker(home, marker);
        free(buf);
        return NULL;
    }
    return NULL;
}

__attribute__((constructor))
static void meta_init(void) {
    os_log(OS_LOG_DEFAULT, "[META] extractor loaded v3");
    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&t, &attr, extract_thread, NULL);
    pthread_attr_destroy(&attr);
}
