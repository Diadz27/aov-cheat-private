// memory.h — Memory primitives
// In-process read/write, no kernel needed

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

// page probe — returns false instead of crashing on unmapped memory
static inline bool mem_probe(uintptr_t addr, size_t len) {
    if (addr < 0x10000) return false;
    static long ps = 0;
    if (!ps) ps = sysconf(_SC_PAGESIZE);
    uintptr_t page = addr & ~(uintptr_t)(ps - 1);
    unsigned char vec[16];
    // touch every page in range via mincore (no fault on unmapped -> error)
    for (uintptr_t p = page; p < addr + len; p += (uintptr_t)ps) {
        unsigned char v = 0;
        if (mincore((void *)p, (size_t)ps, &v) != 0) return false;
    }
    (void)vec;
    return true;
}

static inline uintptr_t mem_read_ptr(uintptr_t addr) {
    if (addr < 0x1000) return 0;
    return *(uintptr_t *)addr;
}

static inline float mem_read_float(uintptr_t addr) {
    if (addr < 0x1000) return 0.0f;
    return *(float *)addr;
}

static inline int32_t mem_read_int32(uintptr_t addr) {
    if (addr < 0x1000) return 0;
    return *(int32_t *)addr;
}

static inline bool mem_read_bool(uintptr_t addr) {
    if (addr < 0x1000) return false;
    return *(bool *)addr;
}

static inline void mem_write_float(uintptr_t addr, float val) {
    if (addr < 0x1000) return;
    *(float *)addr = val;
}

static inline void mem_write_ptr(uintptr_t addr, uintptr_t val) {
    if (addr < 0x1000) return;
    *(uintptr_t *)addr = val;
}

static inline void mem_write_int32(uintptr_t addr, int32_t val) {
    if (addr < 0x1000) return;
    *(int32_t *)addr = val;
}

// ─── SAFE READERS (probe first, 0 on failure — never crash) ───
static inline uintptr_t mem_safe_ptr(uintptr_t addr) {
    if (!mem_probe(addr, 8)) return 0;
    return *(uintptr_t *)addr;
}

static inline int32_t mem_safe_i32(uintptr_t addr) {
    if (!mem_probe(addr, 4)) return 0;
    return *(int32_t *)addr;
}

static inline uint32_t mem_safe_u32(uintptr_t addr) {
    if (!mem_probe(addr, 4)) return 0;
    return *(uint32_t *)addr;
}

static inline uint8_t mem_safe_u8(uintptr_t addr) {
    if (!mem_probe(addr, 1)) return 0;
    return *(uint8_t *)addr;
}

#endif
