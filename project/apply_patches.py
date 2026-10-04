#!/usr/bin/env python3
# apply_patches.py — static binary patcher cho UnityFramework
# Patch tĩnh trước khi ESign resign → hash khớp → chạy được.
# Bảng 4 patches từ bindiff mod 1.64 vs stock 1.64 (mrpewrev/patches_164.txt),
# verify bằng máy cả hai chiều. P3/P4 giữ verify OLD (không skip mù).
# usage: python3 apply_patches.py <UnityFramework> [--dry-run]

import struct
import sys
import shutil
import os

# Bảng patch đọc từ mrpewrev/patches_164.txt (single source of truth,
# machine-generated từ bindiff — cấm gõ tay bytes vào đây).
# format file:
#   PATCH file=0xOFFSET len=N
#     OLD: <hex>
#     NEW: <hex>
TABLE_PATH = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                          "..", "mrpewrev", "patches_164.txt")

PATCH_NAMES = {
    0x04A38100: "P1_vision_param",
    0x0540ECC0: "P2_fog_scale",
    0x0554B9EC: "P3_fog_getter_true",
    0x05ADF5A8: "P4_vision_getter_true",
}


def load_patches():
    patches = []
    with open(TABLE_PATH, "r", encoding="utf-8") as f:
        lines = [l.strip() for l in f if l.strip()]
    i = 0
    while i < len(lines):
        if not lines[i].startswith("PATCH file="):
            i += 1
            continue
        hdr = lines[i].split()
        offset = int(hdr[1].split("=")[1], 16)
        old_hex = lines[i + 1].split(":", 1)[1].strip()
        new_hex = lines[i + 2].split(":", 1)[1].strip()
        old_b, new_b = bytes.fromhex(old_hex), bytes.fromhex(new_hex)
        if len(old_b) != len(new_b):
            print("[PATCHER] ABORT: table len mismatch @ 0x%08X" % offset)
            return None
        patches.append((PATCH_NAMES.get(offset, "UNK"), offset, old_b, new_b))
        i += 3
    if len(patches) != 4:
        print("[PATCHER] ABORT: table has %d patches, want 4" % len(patches))
        return None
    return patches


PATCHES = None  # nạp lúc chạy (xem apply_patches)

EXPECTED_SIZE = 240887264
EXPECTED_MAGIC = 0xFEEDFACF


def apply_patches(binary_path, dry_run=False):
    print("[PATCHER] target: %s" % binary_path)
    print("[PATCHER] size: %d bytes" % os.path.getsize(binary_path))

    patches = load_patches()
    if patches is None:
        return False
    print("[PATCHER] table: %d patches from %s" % (len(patches), TABLE_PATH))

    with open(binary_path, "rb") as f:
        data = bytearray(f.read())

    if struct.unpack_from("<I", data, 0)[0] != EXPECTED_MAGIC:
        print("[PATCHER] ABORT: not ARM64 Mach-O")
        return False
    if len(data) != EXPECTED_SIZE:
        print("[PATCHER] ABORT: size mismatch (want %d, got %d) — sai version?"
              % (EXPECTED_SIZE, len(data)))
        return False

    backup = binary_path + ".bak"
    if not dry_run:
        shutil.copy2(binary_path, backup)
        print("[PATCHER] backup: %s" % backup)

    results = []
    for name, offset, old_bytes, new_bytes in patches:
        size = len(new_bytes)
        if offset + size > len(data):
            print("[PATCHER] %s SKIP: out of bounds" % name)
            results.append((name, "OUT_OF_BOUNDS"))
            continue
        current = bytes(data[offset:offset + size])
        if current != old_bytes:
            if current == new_bytes:
                print("[PATCHER] %s ALREADY PATCHED — skip" % name)
                results.append((name, "ALREADY_PATCHED"))
                continue
            print("[PATCHER] %s MISMATCH" % name)
            print("  expected: %s" % old_bytes.hex())
            print("  found:    %s" % current.hex())
            results.append((name, "MISMATCH"))
            continue
        if dry_run:
            print("[PATCHER] %s DRY_RUN OK @ 0x%08X" % (name, offset))
            results.append((name, "DRY_RUN_OK"))
            continue
        data[offset:offset + size] = new_bytes
        print("[PATCHER] %s PATCHED @ 0x%08X (%dB)" % (name, offset, size))
        results.append((name, "PATCHED"))

    if not dry_run:
        with open(binary_path, "wb") as f:
            f.write(data)
        print("[PATCHER] written: %d bytes" % len(data))

    print("")
    print("[PATCHER] Summary:")
    ok = 0
    for name, status in results:
        icon = "Y" if status in ("PATCHED", "ALREADY_PATCHED", "DRY_RUN_OK") else "X"
        print("  %s %s: %s" % (icon, name, status))
        if status in ("PATCHED", "ALREADY_PATCHED"):
            ok += 1
    success = all(s in ("PATCHED", "ALREADY_PATCHED", "DRY_RUN_OK")
                  for _, s in results)
    print("")
    print("[PATCHER] %s %d/4" % ("SUCCESS" if success else "FAILED", ok))
    return success


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("usage: python3 apply_patches.py <UnityFramework> [--dry-run]")
        sys.exit(1)
    ok = apply_patches(sys.argv[1], dry_run="--dry-run" in sys.argv[2:])
    sys.exit(0 if ok else 1)
