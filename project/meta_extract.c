// meta_extract.c v2 — decrypted metadata extractor
// Quét bộ nhớ tìm global-metadata.dat đã giải mã (magic 0xFAB11BAF),
// lưu ra Documents. Log bằng os_log (thấy được trong Console/3uTools).
// Compile: xcrun -sdk iphoneos clang -arch arm64 -dynamiclib
//          -framework Foundation -o meta_extract.dylib meta_extract.c
// *find the decrypted heart beating in memory, copy it out*

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
#define MAX_REGION_COPY (512ULL * 1024 * 1024)

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
    // stringOffset/stringCount ở +28/+32 trong mọi version >= 16
    if (!safe_read_u32(addr + 28, &s_off)) return false;
    if (!safe_read_u32(addr + 32, &s_cnt)) return false;
    if (s_off > 200 * 1024 * 1024 || s_cnt > 20 * 1024 * 1024) return false;
    if (ver_out) *ver_out = ver;
    return true;
}

// luôn để lại marker trong Documents (sống/chết đều có dấu)
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
    LOG("scan start (public delay=%{public}d)", INIT_DELAY);

    const char *home = getenv("HOME");
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
            size <= MAX_REGION_COPY) {
            for (vm_address_t p = addr;
                 p + 4096 <= addr + size && p < addr + 16 * 1024 * 1024;
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
    LOG("scan done regions=%{public}lu best=%{public}lx", regions, (unsigned long)best);

    char marker[256];
    if (!best) {
        LOG("NOT FOUND");
        snprintf(marker, sizeof(marker), "NOT FOUND regions=%lu\n", regions);
        write_marker(home, marker);
        return NULL;
    }

    uint8_t *buf = malloc(best_size);
    if (!buf) {
        LOG("MALLOC FAIL size=%{public}llu", (unsigned long long)best_size);
        snprintf(marker, sizeof(marker), "MALLOC FAIL size=%llu\n",
                 (unsigned long long)best_size);
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
        // FIX tràn heap: vm_read có thể trả cc > want (làm tròn page)
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
    LOG("%{public}s (%{public}zu bytes, ver=%{public}u)",
        wrt ? "SAVED" : "WRITE FAILED", done, best_ver);
    snprintf(marker, sizeof(marker), "%s bytes=%zu ver=%u regions=%lu\n",
             wrt ? "SAVED" : "WRITE FAILED", done, best_ver, regions);
    write_marker(home, marker);
    free(buf);
    return NULL;
}

__attribute__((constructor))
static void meta_init(void) {
    os_log(OS_LOG_DEFAULT, "[META] extractor loaded v2");
    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&t, &attr, extract_thread, NULL);
    pthread_attr_destroy(&attr);
}
