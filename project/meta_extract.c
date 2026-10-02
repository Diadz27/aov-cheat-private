// meta_extract.c — decrypted metadata extractor (Phase 3a)
// Chạy in-process trong game: quét bộ nhớ tìm global-metadata.dat
// đã giải mã (magic 0xFAB11BAF), lưu ra Documents để pull về dump tĩnh.
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
#include <mach/mach.h>
#include <mach/vm_region.h>
#include <mach-o/dyld.h>

#define META_MAGIC   0xFAB11BAF
#define META_MIN_VER 16
#define META_MAX_VER 31
#define INIT_DELAY   10

// đọc an toàn 1 uint32 (tránh crash khi page guard)
static bool safe_read_u32(uintptr_t addr, uint32_t *out) {
    vm_offset_t mem = 0;
    mach_msg_type_number_t cnt = 0;
    kern_return_t kr = vm_read(mach_task_self(), (vm_address_t)addr, 4, &mem, &cnt);
    if (kr != KERN_SUCCESS) return false;
    memcpy(out, (void *)mem, 4);
    vm_deallocate(mach_task_self(), mem, cnt);
    return true;
}

// kiểm tra header plausibility tại addr
static bool looks_like_metadata(uintptr_t addr, uint32_t *ver_out,
                                uint32_t *str_off_out, uint32_t *str_cnt_out) {
    uint32_t magic = 0, ver = 0, s_off = 0, s_cnt = 0;
    if (!safe_read_u32(addr, &magic) || magic != META_MAGIC) return false;
    if (!safe_read_u32(addr + 4, &ver)) return false;
    if (ver < META_MIN_VER || ver > META_MAX_VER) return false;
    // stringOffset/stringCount nằm ở +28/+32 trong mọi version >= 16
    if (!safe_read_u32(addr + 28, &s_off)) return false;
    if (!safe_read_u32(addr + 32, &s_cnt)) return false;
    if (s_off > 200 * 1024 * 1024 || s_cnt > 20 * 1024 * 1024) return false;
    if (ver_out) *ver_out = ver;
    if (str_off_out) *str_off_out = s_off;
    if (str_cnt_out) *str_cnt_out = s_cnt;
    return true;
}

static void *extract_thread(void *arg) {
    (void)arg;
    sleep(INIT_DELAY);
    printf("[META] scan start\n");

    vm_address_t addr = 0;
    vm_size_t size = 0;
    uintptr_t best = 0;
    vm_size_t best_size = 0;
    uint32_t best_ver = 0;

    while (1) {
        vm_region_basic_info_data_64_t info;
        mach_msg_type_number_t cnt = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t obj = MACH_PORT_NULL;
        kern_return_t kr = vm_region_64(mach_task_self(), &addr, &size,
                                        VM_REGION_BASIC_INFO_64,
                                        (vm_region_info_t)&info, &cnt, &obj);
        if (kr != KERN_SUCCESS) break;
        if ((info.protection & VM_PROT_READ) && size >= 1024 * 1024) {
            // quét từng page đầu (magic nằm ở đầu blob)
            for (vm_address_t p = addr;
                 p + 4096 <= addr + size && p < addr + 16 * 1024 * 1024;
                 p += 4096) {
                uint32_t ver = 0, s_off = 0, s_cnt = 0;
                if (looks_like_metadata((uintptr_t)p, &ver, &s_off, &s_cnt)) {
                    printf("[META] HIT va=0x%lx ver=%u str_off=%u str_cnt=%u region=%lluMB\n",
                           (unsigned long)p, ver, s_off, s_cnt,
                           (unsigned long long)size / 1024 / 1024);
                    if (size > best_size) {
                        best = (uintptr_t)p;
                        best_size = size;
                        best_ver = ver;
                    }
                    break; // 1 hit mỗi region là đủ
                }
            }
        }
        addr += size;
        if (addr == 0) break; // wrap
    }

    if (!best) {
        printf("[META] NOT FOUND — thử tăng INIT_DELAY hoặc chạy lại khi vào trận\n");
        return NULL;
    }

    // copy toàn bộ region chứa metadata
    uint8_t *buf = malloc(best_size);
    if (!buf) return NULL;
    vm_offset_t mem = 0;
    mach_msg_type_number_t cc = 0;
    // copy theo chunk 1MB cho chắc
    size_t done = 0;
    while (done < best_size) {
        size_t want = best_size - done > 1024 * 1024 ? 1024 * 1024 : best_size - done;
        if (vm_read(mach_task_self(), (vm_address_t)(best + done), want, &mem, &cc) != KERN_SUCCESS) {
            // region có guard page — cắt ở đây, phần đầu (header + tables) thường đã đủ
            printf("[META] guard at +%zuMB, truncating\n", done / 1024 / 1024);
            break;
        }
        memcpy(buf + done, (void *)mem, cc);
        vm_deallocate(mach_task_self(), mem, cc);
        done += want;
    }

    // đường ra: $HOME/Documents (pure C, không ObjC)
    const char *home = getenv("HOME");
    char path[1024];
    snprintf(path, sizeof(path), "%s/Documents/global-metadata.decrypted.dat",
             home ? home : "/tmp");
    FILE *f = fopen(path, "wb");
    bool wrt = false;
    if (f) {
        wrt = (fwrite(buf, 1, done, f) == done);
        fclose(f);
    }
    printf("[META] %s (%zu bytes, ver=%u) -> %s\n",
           wrt ? "SAVED" : "WRITE FAILED", done, best_ver, path);
    free(buf);
    return NULL;
}

__attribute__((constructor))
static void meta_init(void) {
    printf("[META] extractor loaded — iOS ARM64\n");
    pthread_t t;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&t, &attr, extract_thread, NULL);
    pthread_attr_destroy(&attr);
}
