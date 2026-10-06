#!/usr/bin/env python3
# slot_parser.py — getter RVA -> static slot (absolute adrp page + imm)
# Usage: python slot_parser.py <UnityFramework> <RVA_hex> [n_insns]
# Rule: adrp prints ABSOLUTE page (never +PC). Only full-width ldr/str/add
# count (ldrb/strb are 1B init-flags, not slots). Slot must land in __DATA*.
WIDE = ('add', 'ldr', 'str')
# Exit != 0 with NOVELTY-STOP if ambiguous or outside data segments.
import struct
import sys

try:
    from capstone import Cs, CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN
except ImportError:
    print('need: pip install capstone')
    sys.exit(3)


def segments(path):
    d = open(path, 'rb').read()
    assert struct.unpack_from('<I', d, 0)[0] == 0xFEEDFACF, 'not thin ARM64'
    ncmds = struct.unpack_from('<I', d, 16)[0]
    p, segs = 32, []
    for _ in range(ncmds):
        cmd, sz = struct.unpack_from('<2I', d[p:p + 8])
        if cmd == 0x19:
            name = d[p + 8:p + 24].split(b'\x00')[0].decode()
            vm, vs, fo, fs = struct.unpack('<4Q', d[p + 24:p + 56])
            segs.append((name, vm, fo, fs))
        p += sz
    return d, segs


def in_data(segs, addr):
    for name, vm, fo, fs in segs:
        if name.startswith('__DATA') and fo <= addr < fo + fs:
            return True
    return False


def parse_slot(path, rva, n=40):
    d, segs = segments(path)
    md = Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)
    code = d[rva:rva + n * 4]
    ins = list(md.disasm(code, rva))
    cands = []
    for i, insn in enumerate(ins[:-1]):
        if insn.mnemonic == 'adrp':
            parts0 = insn.op_str.split(',')
            reg = parts0[0].strip()
            page = int(parts0[1].strip().lstrip('#'), 16)
            nxt = ins[i + 1]
            uses_base = ('[%s,' % reg) in nxt.op_str
            uses_add = nxt.mnemonic == 'add' and nxt.op_str.startswith(reg + ',')
            if nxt.mnemonic in WIDE and (uses_base or uses_add):
                parts = nxt.op_str.split('#')
                if len(parts) == 2:
                    try:
                        imm = int(parts[1].split(']')[0].strip().rstrip(',]').strip(), 16)
                    except ValueError:
                        continue
                    cands.append((insn.address, page + imm, nxt.mnemonic))
    for addr, slot, how in cands:
        print('cand @0x%X slot=0x%X via=%s in_data=%s' % (addr, slot, how, in_data(segs, slot)))
    data_slots = sorted(set(s for _, s, _ in cands if in_data(segs, s)))
    if len(data_slots) != 1:
        print('NOVELTY-STOP: %d __DATA slot candidates -- manual RE required' % len(data_slots))
        return 1
    print('SLOT=0x%X' % data_slots[0])
    return 0


def main():
    if len(sys.argv) < 3:
        print('usage: slot_parser.py <UnityFramework> <RVA_hex> [n]')
        return 1
    return parse_slot(sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3]) if len(sys.argv) > 3 else 40)


if __name__ == '__main__':
    sys.exit(main())
