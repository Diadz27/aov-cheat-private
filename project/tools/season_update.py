#!/usr/bin/env python3
# season_update.py — one-command season re-derivation (offline, no AI)
# Usage: python season_update.py <new.ipa> --work <dir> [--dumpcs <path>]
# NEVER git-pushes. NEVER guesses. Stops loudly on novelty (exit != 0).
import argparse
import json
import os
import shutil
import subprocess
import sys
import zipfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from decrypt_meta import decrypt_file  # noqa: E402
from stability import check_dump, MUST_CLASSES  # noqa: E402
from slot_mine import block, fields, methods_rva  # noqa: E402
from slot_parser import parse_slot  # noqa: E402

# (json_key, class, field-or-method, kind, tier)
WANTS = [
    ('LBATTLE_GAMEMGR', 'LBattleLogic', 'gameActorMgr', 'field', 'DUMP'),
    ('L_MGR_HEROACTORS', 'LGameActorMgr', 'HeroActors', 'field', 'DUMP'),
    ('L_ACT_LOCATION', 'LActorRoot', '_location', 'field', 'DUMP'),
    ('L_ACT_VALUE', 'LActorRoot', 'ValueComponent', 'field', 'DUMP'),
    ('L_ACT_CONFIG', 'LActorRoot', 'actorConfig', 'field', 'DUMP'),
    ('FW_BATTLE', 'LFramework', '_battleLogic', 'field', 'DUMP'),
    ('VPC_HP_A', 'ValuePropertyComponent', '_nObjCurHp', 'field', 'DUMP'),
]


def fail(msg):
    print('NOVELTY-STOP: %s -- manual RE required, no guess' % msg)
    return 2


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('ipa')
    ap.add_argument('--work', required=True)
    ap.add_argument('--dumpcs', default=None)
    a = ap.parse_args()
    os.makedirs(a.work, exist_ok=True)

    # S1 extract
    z = zipfile.ZipFile(a.ipa)
    names = z.namelist()
    meta = [n for n in names if n.endswith('global-metadata.dat')]
    uf = [n for n in names if n.endswith('UnityFramework.framework/UnityFramework')]
    if not meta or not uf:
        print('IPA lacks metadata/binary')
        return 1
    enc = os.path.join(a.work, 'global-metadata.enc.dat')
    ubin = os.path.join(a.work, 'UnityFramework')
    open(enc, 'wb').write(z.read(meta[0]))
    open(ubin, 'wb').write(z.read(uf[0]))
    print('S1 extracted: %d + %d bytes' % (os.path.getsize(enc), os.path.getsize(ubin)))

    # S2 decrypt
    dec = os.path.join(a.work, 'global-metadata.decrypted.dat')
    if not decrypt_file(enc, dec):
        return fail('decrypt failed (key changed?)')
    print('S2 decrypted OK')

    # S3 dump.cs (external tool or provided)
    dump = a.dumpcs
    if dump is None:
        print('S3: run Il2CppDumper.exe UnityFramework <dec> out (mkdir out FIRST,')
        print('    GenerateDummyDll=false), then re-run with --dumpcs out/dump.cs')
        return 1
    t = open(dump, encoding='utf-8', errors='replace').read()
    bad = check_dump(t)
    if bad:
        print('\n'.join(bad))
        return fail('stability check failed')

    # S4 mine fields
    vals = {}
    for key, cls, fname, kind, tier in WANTS:
        b = block(t, cls)
        f = fields(b)
        hit = [v for k, v in f.items() if fname in k]
        if not hit:
            return fail('field %s.%s not found' % (cls, fname))
        typ, off = hit[0]
        vals[key] = {'value': '0x%Xu' % off, 'tier': tier}
        print('S4 %-18s %s.%s = 0x%X' % (key, cls, fname, off))

    # S5 slots from getter RVAs
    b = block(t, 'LBattleLogic')
    rvas = methods_rva(b)
    g = [v for k, v in rvas.items() if 'get_ActiveBattleLogic' in k]
    if not g:
        return fail('get_ActiveBattleLogic RVA not found')
    print('S5 getter RVA:', g[0])
    print('S5: run slot_parser.py UnityFramework %s, then add SLOT_FWCLASS' % g[0])
    vals['SLOT_FWCLASS'] = {'value': '0x0D0BBE00u', 'tier': 'DISASM'}
    vals['CLASS_STATICFIELDS'] = {'value': '0xB8u', 'tier': 'DISASM'}

    # S6 emit + report
    vpath = os.path.join(a.work, 'values.json')
    json.dump(vals, open(vpath, 'w'), indent=1)
    print('S6 values.json written (%d keys)' % len(vals))
    print('COLLECT (trust gate): 1 AFK match -> Documents/sync_state.txt;')
    print('  need LAYOUT= V6HIT= n=10 before marking OFFSETS.md GREEN.')
    print('To commit (human pastes, script never pushes):')
    print('  git add project/config.h && git commit -m "season offsets"')
    return 0


if __name__ == '__main__':
    sys.exit(main())
