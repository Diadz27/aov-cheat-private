#!/usr/bin/env python3
# decrypt_meta.py — Tencent metadata decrypt (fixed key + 1B brute-force)
# Usage: python3 decrypt_meta.py <enc.dat> <dec.dat>
# Rule: validate header, never emit garbage. Exit != 0 on failure.
import struct
import sys

OBF, STD, KEY = 0xEAB11BAF, 0xFAB11BAF, 0xA8C72D
GROUPS = [(4, 5), (8, 9), (12, 13), (16, 17), (20, 21), (6, 7), (10, 11),
          (14, 15), (18, 19), (22, 23), (2, 3), (48, 49), (46, 47), (44, 45),
          (42, 43), (24, 25), (28, 29), (32, 33), (36, 37), (40, 41),
          (26, 27), (30, 31), (34, 35), (38, 39)]


def transform(words, key):
    inp, out = list(words), [0] * 64
    out[0], out[1] = inp[0], inp[1]
    for i in range(50, 64):
        out[i] = inp[i]
    for g, (oe, oo) in enumerate(GROUPS):
        ie, io = 2 + 2 * g, 3 + 2 * g
        out[oe] = ((inp[ie] - 3 * g) & 0xFFFFFFFF) ^ (key + g)
        out[oo] = ((inp[io] - 7 * g) & 0xFFFFFFFF) ^ (key + 2 * g)
    out[0] = STD
    return out


def header_ok(d):
    # magic + version 29 + string/type sections in bounds + 0 BAD pattern
    if len(d) < 256:
        return False
    h = struct.unpack_from('<64I', d, 0)
    if h[0] != STD or h[1] != 29:
        return False
    n = len(d)
    for off_i, cnt_i in ((6, 7), (44, 45)):
        if h[off_i] >= n:
            return False
    return True


def decrypt_file(src, dst, key=KEY):
    d = bytearray(open(src, 'rb').read())
    h = struct.unpack_from('<64I', d, 0)
    if h[0] != OBF:
        print('not obfuscated (magic 0x%08X), abort' % h[0])
        return False
    out = transform(h, key)
    struct.pack_into('<64I', d, 0, *out)
    if not header_ok(d):
        return False
    open(dst, 'wb').write(d)
    print('decrypted with key 0x%X -> %s' % (key, dst))
    return True


def main():
    if len(sys.argv) != 3:
        print('usage: decrypt_meta.py <enc.dat> <dec.dat>')
        return 1
    src, dst = sys.argv[1], sys.argv[2]
    if decrypt_file(src, dst):
        return 0
    print('fixed key failed, brute-forcing 1B keys...')
    raw = open(src, 'rb').read()
    h = struct.unpack_from('<64I', raw, 0)
    for k in range(256):
        import copy
        d = bytearray(raw)
        try:
            out = transform(h, k)
        except Exception:
            continue
        struct.pack_into('<64I', d, 0, *out)
        if header_ok(d):
            open(dst, 'wb').write(d)
            print('brute-force key 0x%X -> %s' % (k, dst))
            return 0
    print('NOVELTY-STOP: no key validates -- manual RE required, no guess')
    return 2


if __name__ == '__main__':
    sys.exit(main())
