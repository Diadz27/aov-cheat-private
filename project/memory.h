// memory.h — Memory primitives (in-process, no kernel)
// L-rules: probe before every read; single-copy reads (no TOCTOU split);
// writers compiled out unless ENABLE_WRITES.

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

// page probe — false instead of crash on unmapped memory
static inline bool mem_probe(uintptr_t addr, size_t len) {
    if (addr < 0x10000) return false;
    if (len == 0 || len > 0x10000) return false;
    static long ps = 0;
    if (!ps) ps = sysconf(_SC_PAGESIZE);
    if (ps <= 0) return false;
    uintptr_t end = addr + len;
    if (end < addr) return false; // overflow
    uintptr_t page = addr & ~(uintptr_t)(ps - 1);
    for (uintptr_t p = page; p < end; p += (uintptr_t)ps) {
        unsigned char v = 0;
        if (mincore((void *)p, (size_t)ps, &v) != 0) return false;
    }
    return true;
}

// ─── legacy raw readers (kept for compat; prefer safe_* below) ───
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

#ifdef ENABLE_WRITES
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
#endif

// ─── SAFE READERS (probe first, 0 on failure — never crash) ───
static inline uintptr_t mem_safe_ptr(uintptr_t addr) {
    if (!mem_probe(addr, 8)) return 0;
    return *(uintptr_t *)addr;
}

static inline uint64_t mem_safe_u64(uintptr_t addr) {
    if (!mem_probe(addr, 8)) return 0;
    uint64_t v = 0;
    memcpy(&v, (void *)addr, 8); // single copy
    return v;
}

static inline int32_t mem_safe_i32(uintptr_t addr) {
    if (!mem_probe(addr, 4)) return 0;
    int32_t v = 0;
    memcpy(&v, (void *)addr, 4);
    return v;
}

static inline uint32_t mem_safe_u32(uintptr_t addr) {
    if (!mem_probe(addr, 4)) return 0;
    uint32_t v = 0;
    memcpy(&v, (void *)addr, 4);
    return v;
}

static inline uint16_t mem_safe_u16(uintptr_t addr) {
    if (!mem_probe(addr, 2)) return 0;
    uint16_t v = 0;
    memcpy(&v, (void *)addr, 2);
    return v;
}

static inline uint8_t mem_safe_u8(uintptr_t addr) {
    if (!mem_probe(addr, 1)) return 0;
    return *(uint8_t *)addr;
}

// single-copy block read; false if unreadable
static inline bool mem_copy_bytes(uintptr_t addr, void *out, size_t len) {
    if (!out || !mem_probe(addr, len)) return false;
    memcpy(out, (void *)addr, len);
    return true;
}

#endif
