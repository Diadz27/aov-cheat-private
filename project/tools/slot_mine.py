#!/usr/bin/env python3
# slot_mine.py — dump.cs class/field-offset miner
# Usage: python slot_mine.py <dump.cs> <ClassName>...  -> prints fields
# Rule: brace-match blocks, parse `Type Name; // 0xOFF` from //Fields section.
import re
import sys


def block(t, name, base=None):
    # (?![\w.]) rejects nested prefixes like LGameActorMgr.stX.
    # base= filters decoys (e.g. ActorConfigData: want LEngineComponent one).
    pat = (r'(?:public |internal |private |protected |)(?:sealed |abstract |static |partial )*'
           r'(?:class|struct|interface|enum) ' + re.escape(name) + r'(?![\w.])([^\n]*)\n\{')
    for m in re.finditer(pat, t):
        if base is not None and (': ' + base) not in m.group(1):
            continue
        i = m.start()
        j = t.find('{', m.end() - 1)
        depth = 0
        while j < len(t):
            if t[j] == '{':
                depth += 1
            elif t[j] == '}':
                depth -= 1
                if depth == 0:
                    return t[i:j + 1]
            j += 1
        return None
    return None


def fields(b):
    fs = b.find('// Fields')
    if fs < 0:
        return {}
    fe = b.find('// Properties', fs)
    if fe < 0:
        fe = b.find('// Methods', fs)
    seg = b[fs:fe if fe > 0 else fs + 6000]
    # cut nested types (their fields have their own offsets)
    nm = re.search(r'^\s*(?:public |internal |private |protected |sealed |abstract |static |partial )*(?:class|struct|interface|enum) \w', seg, re.M)
    if nm:
        seg = seg[:nm.start()]
    out = {}
    # greedy type (last word before ';' is the name): handles 'static T name'
    for m in re.finditer(r'^\s*(?:public|private|protected|internal)[^;\n]*?\b([\w<>,\. ]+)\s+[<]?([\w]+)[^;\n]*?;\s*//\s*0x([0-9A-Fa-f]+)', seg, re.M):
        typ, fname, off = m.group(1).strip(), m.group(2), int(m.group(3), 16)
        # normalize <x>k__BackingField -> x
        bm = re.match(r'<(.+)>k__BackingField', fname)
        if bm:
            fname = bm.group(1)
        out[fname] = (typ, off)
    return out


def methods_rva(b):
    # decls end with `{ }` on the same line — capture the full signature.
    out = {}
    for m in re.finditer(r'// RVA: (0x[0-9A-Fa-f]+)[^\n]*\n\t((?:public|private|protected|internal)[^\n]*?\([^)]*\))', b):
        out[m.group(2).strip()] = m.group(1)
    return out


def main():
    if len(sys.argv) < 3:
        print('usage: slot_mine.py <dump.cs> <Class>...')
        return 1
    t = open(sys.argv[1], encoding='utf-8', errors='replace').read()
    for name in sys.argv[2:]:
        b = block(t, name)
        if b is None:
            print('== %s NOT-FOUND' % name)
            continue
        print('== %s len=%d' % (name, len(b)))
        for fname, (typ, off) in fields(b).items():
            print('  F %-28s %-24s 0x%X' % (fname, typ[:24], off))
    return 0


if __name__ == '__main__':
    sys.exit(main())
