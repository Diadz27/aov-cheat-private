// memory.h — Memory primitives
// In-process read/write, no kernel needed

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

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

#endif
