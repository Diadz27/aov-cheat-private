// memory.h — Memory primitives (in-process, no kernel)
// L-rules: EVERY read goes through mach_vm_read_overwrite (single kernel
// copy — a racing unmap returns error, never SIGSEGV). No probe-then-deref
// anywhere on the hot path (that race crashed v2 during pack download).
// Writers compiled out unless ENABLE_WRITES.

#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

// kernel copy; false on any failure (unmapped/protection/race). Never faults.
static inline bool mem_vm_read(uintptr_t addr, void *out, size_t len) {
    if (addr < 0x10000 || !out || !len || len > 0x10000) return false;
    mach_vm_size_t got = 0;
    kern_return_t kr = mach_vm_read_overwrite(mach_task_self(),
        (mach_vm_address_t)addr, (mach_vm_size_t)len,
        (mach_vm_address_t)out, &got);
    return kr == KERN_SUCCESS && got == (mach_vm_size_t)len;
}

// page probe — safe by itself (mincore never faults). Used ONLY for coarse
// region gating (v6 chunks), NEVER as a pre-read gate for deref.
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

// ─── SAFE READERS (kernel copy, 0 on failure — never crash) ───
static inline uintptr_t mem_safe_ptr(uintptr_t addr) {
    uintptr_t v = 0;
    mem_vm_read(addr, &v, 8);
    return v;
}

static inline uint64_t mem_safe_u64(uintptr_t addr) {
    uint64_t v = 0;
    mem_vm_read(addr, &v, 8);
    return v;
}

static inline int32_t mem_safe_i32(uintptr_t addr) {
    int32_t v = 0;
    mem_vm_read(addr, &v, 4);
    return v;
}

static inline uint32_t mem_safe_u32(uintptr_t addr) {
    uint32_t v = 0;
    mem_vm_read(addr, &v, 4);
    return v;
}

static inline uint16_t mem_safe_u16(uintptr_t addr) {
    uint16_t v = 0;
    mem_vm_read(addr, &v, 2);
    return v;
}

static inline uint8_t mem_safe_u8(uintptr_t addr) {
    uint8_t v = 0;
    mem_vm_read(addr, &v, 1);
    return v;
}

// single-copy block read; false if unreadable
static inline bool mem_copy_bytes(uintptr_t addr, void *out, size_t len) {
    if (!out) return false;
    return mem_vm_read(addr, out, len);
}

#endif
