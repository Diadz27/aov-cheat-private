#!/usr/bin/env python3
# selftest_164.py — acceptance: pipeline reproduces committed 1.64 config
# Usage: python selftest_164.py   (uses TEMP 1.64 files, read-only)
# A1 decrypt -> A2 mine asserts -> A3 slot -> A4 emit-vs-committed -> A5 report
import io
import json
import os
import re
import struct
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from decrypt_meta import decrypt_file, header_ok  # noqa: E402
from stability import check_dump  # noqa: E402
from slot_mine import block, fields, methods_rva  # noqa: E402
from slot_parser import segments, in_data  # noqa: E402
from capstone import Cs, CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN  # noqa: E402

T = 'C:/Users/pc/AppData/Local/Temp/aov164'
BIN = T + '/UnityFramework'
DUMP = T + '/d2/out29/dump.cs'
WS = 'D:/LQMB BY CLAUDE'
fails = []


def check(name, cond, extra=''):
    print(('PASS ' if cond else 'FAIL ') + name, extra)
    if not cond:
        fails.append(name)


def main():
    # A1 decrypt stock metadata
    enc = T + '/meta_enc.dat'
    if not os.path.exists(enc):
        enc = T + '/Payload/kgvn.app/Data/Managed/Metadata/global-metadata.dat'
    tmp = tempfile.mktemp(suffix='.dat')
    ok = decrypt_file(enc, tmp)
    d = open(tmp, 'rb').read()
    os.remove(tmp)
    check('A1-decrypt', ok and header_ok(d))

    # A2 mine asserts
    t = open(DUMP, encoding='utf-8', errors='replace').read()
    check('A2-stability', not check_dump(t))
    exp = {(None, 'LBattleLogic', 'gameActorMgr'): 0xF8,
           (None, 'LGameActorMgr', 'HeroActors'): 0x48,
           (None, 'LActorRoot', '_location'): 0xE0,
           (None, 'LActorRoot', 'ValueComponent'): 0x338,
           (None, 'LActorRoot', 'actorConfig'): 0x378,
           (None, 'LFramework', '_battleLogic'): 0x68,
           (None, 'ValuePropertyComponent', '_nObjCurHp'): 0x50,
           ('LEngineComponent', 'ActorConfigData', 'CmpType'): 0x38}
    for (base, cls, fname), want in exp.items():
        f = fields(block(t, cls, base))
        hit = [v for k, v in f.items() if fname in k]
        got = hit[0][1] if hit else None
        check('A2-%s.%s' % (cls, fname), got == want, 'got=%s want=0x%X' % (hex(got) if got is not None else None, want))

    # A3 slot from getter RVA (getter lives on LFrameworkEditorProxy, not LBattleLogic)
    b = block(t, 'LFrameworkEditorProxy')
    g = [v for k, v in methods_rva(b).items() if 'get_ActiveBattleLogic' in k]
    check('A3-getter-found', bool(g), g[0] if g else '')
    if g:
        rva = int(g[0], 16)
        bd, segs = segments(BIN)
        md = Cs(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN)
        slots = set()
        ins = list(md.disasm(bd[rva:rva + 60 * 4], rva))
        for i, insn in enumerate(ins[:-1]):
            if insn.mnemonic == 'adrp':
                p0 = insn.op_str.split(',')
                reg, page = p0[0].strip(), int(p0[1].strip().lstrip('#'), 16)
                nx = ins[i + 1]
                use = ('[%s,' % reg) in nx.op_str or (nx.mnemonic == 'add' and nx.op_str.startswith(reg + ','))
                if nx.mnemonic in ('add', 'ldr', 'str') and use and '#' in nx.op_str:
                    imm = int(nx.op_str.split('#')[1].split(']')[0].strip(), 16)
                    if in_data(segs, page + imm):
                        slots.add(page + imm)
        check('A3-slot', slots == {0xD0BBE00}, 'got=%s' % sorted(hex(s) for s in slots))

    # A4 emit vs committed config.h (selected keys)
    committed = open(WS + '/project/config.h', encoding='utf-8').read()
    for key, want in [('LBATTLE_GAMEMGR', '0xF8'), ('L_MGR_HEROACTORS', '0x48'),
                      ('SLOT_FWCLASS', '0x0D0BBE00'), ('VPC_HP_A', '0x50'),
                      ('CFG_CAMP', '0x38')]:
        m = re.search(r'#define\s+' + key + r'\s+(0x[0-9A-Fa-f]+)', committed)
        check('A4-' + key, m and m.group(1).upper().rstrip('U') == want.upper().rstrip('U'),
              'committed=%s' % (m.group(1) if m else None))

    print('SELFTEST %s (%d fails)' % ('GREEN' if not fails else 'RED', len(fails)))
    return 0 if not fails else 1


if __name__ == '__main__':
    sys.exit(main())
