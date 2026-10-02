#!/usr/bin/env python3
# repack_ipa.py — inject cheat.dylib vào UnityFramework trong AOV.ipa
# usage: python3 repack_ipa.py input.ipa cheat.dylib output.ipa [--launcher]
#
# Mặc định target UnityFramework (game thật, 235MB, cryptid=0).
# Stub launcher 71KB trong Payload/*.app KHÔNG phải game — inject vào nó vô nghĩa.
# --launcher: ép target stub launcher (không khuyến nghị).
#
# Khác bản gốc: KHÔNG chèn byte giữa header (làm lệch toàn bộ file offset).
# Ghi LC_LOAD_DYLIB in-place vào header slack. Hết slack → báo lỗi, không phá binary.
# Sau inject vẫn phải ký lại (ldid -S) trên mac — script không làm thay.

import sys, os, shutil, zipfile, struct, tempfile

MH_MAGIC_64          = 0xFEEDFACF
CPU_TYPE_ARM64       = 0x0100000C
LC_SEGMENT_64        = 0x19
LC_LOAD_DYLIB        = 0x0C
LC_ENCRYPTION_64     = 0x2C
LC_CODE_SIGNATURE    = 0x1D


def align_up(n, a=8):
    return (n + a - 1) & ~(a - 1)


def find_unity_binary(payload_dir):
    for d in os.listdir(payload_dir):
        if not d.endswith('.app'):
            continue
        cand = os.path.join(payload_dir, d, 'Frameworks',
                            'UnityFramework.framework', 'UnityFramework')
        if os.path.isfile(cand):
            return cand, os.path.join(payload_dir, d)
    return None, None


def find_launcher_binary(payload_dir):
    for d in os.listdir(payload_dir):
        if d.endswith('.app'):
            app = os.path.join(payload_dir, d)
            binary = os.path.join(app, d[:-4])
            if os.path.isfile(binary):
                return binary, app
    return None, None


def parse_header(data):
    magic = struct.unpack_from('<I', data, 0)[0]
    if magic != MH_MAGIC_64:
        return None, f'Not thin ARM64 Mach-O: {hex(magic)}'
    cputype = struct.unpack_from('<I', data, 4)[0]
    if cputype != CPU_TYPE_ARM64:
        return None, f'Not ARM64 cputype: {hex(cputype)}'
    ncmds, sizeofcmds = struct.unpack_from('<II', data, 16)
    off = 32
    cmds = []
    min_fileoff = None
    cryptid = None
    has_sig = False
    for _ in range(ncmds):
        cmd, cmdsize = struct.unpack_from('<II', data, off)
        cmds.append((cmd, off, cmdsize))
        if cmd == LC_SEGMENT_64:
            fileoff = struct.unpack_from('<Q', data, off + 40)[0]
            if fileoff > 0 and (min_fileoff is None or fileoff < min_fileoff):
                min_fileoff = fileoff
        elif cmd == LC_ENCRYPTION_64:
            cryptid = struct.unpack_from('<I', data, off + 16)[0]
        elif cmd == LC_CODE_SIGNATURE:
            has_sig = True
        off += cmdsize
    info = {
        'ncmds': ncmds, 'sizeofcmds': sizeofcmds,
        'header_end': 32 + sizeofcmds,
        'min_fileoff': min_fileoff, 'cryptid': cryptid,
        'has_sig': has_sig,
    }
    return info, None


def inject_dylib(binary_path, dylib_install):
    with open(binary_path, 'rb') as f:
        data = bytearray(f.read())

    info, err = parse_header(data)
    if err:
        print(f'[!] {err}')
        return False

    if info['cryptid']:
        print(f'[!] Binary còn mã hóa FairPlay (cryptid={info["cryptid"]}) — '
              f'cần bản decrypted trước, dừng lại')
        return False

    name_bytes = dylib_install.encode('utf-8') + b'\x00'
    name_bytes += b'\x00' * ((8 - len(name_bytes) % 8) % 8)
    cmdsize = 24 + len(name_bytes)

    if info['min_fileoff'] is None:
        print('[!] Không tìm được segment fileoff — binary lạ, dừng lại')
        return False
    slack = info['min_fileoff'] - info['header_end']
    print(f'[*] ncmds={info["ncmds"]} sizeofcmds={info["sizeofcmds"]} '
          f'header_end={info["header_end"]} min_fileoff={info["min_fileoff"]} '
          f'slack={slack} cần={cmdsize}')
    if cmdsize > slack:
        print(f'[!] Hết header slack ({slack} < {cmdsize}) — '
              f'cần optool/insert_dylib trên mac, dừng lại an toàn')
        return False

    lc = struct.pack('<II', LC_LOAD_DYLIB, cmdsize)
    lc += struct.pack('<IIII', 24, 2, 0, 0)  # name_offset, timestamp, cur, compat
    lc += name_bytes

    pos = info['header_end']
    data[pos:pos + cmdsize] = lc
    struct.pack_into('<I', data, 16, info['ncmds'] + 1)
    struct.pack_into('<I', data, 20, info['sizeofcmds'] + cmdsize)

    with open(binary_path, 'wb') as f:
        f.write(data)

    print(f'[+] Injected in-place: {dylib_install}')
    if info['has_sig']:
        print('[!] Binary có chữ ký cũ — NHỚ ký lại: ldid -S <binary> (trên mac)')
    return True


def repack(input_ipa, dylib_path, output_ipa, use_launcher=False):
    tmp = tempfile.mkdtemp()
    try:
        print(f'[*] Extracting {input_ipa}')
        with zipfile.ZipFile(input_ipa, 'r') as z:
            z.extractall(tmp)

        payload = os.path.join(tmp, 'Payload')
        if use_launcher:
            binary, app_dir = find_launcher_binary(payload)
            print('[*] Target: LAUNCHER (theo --launcher)')
        else:
            binary, app_dir = find_unity_binary(payload)
            print('[*] Target: UnityFramework')
        if not binary:
            # FALLBACK: stub launcher (ít dùng, cảnh báo rõ)
            binary, app_dir = find_launcher_binary(payload)
            if binary:
                print('[!] Warning: UnityFramework not found, '
                      'using stub launcher as target')
        if not binary:
            print('[!] Binary not found')
            return False
        print(f'[*] Binary: {binary}')

        fw_dir = os.path.join(app_dir, 'Frameworks')
        os.makedirs(fw_dir, exist_ok=True)
        dylib_dest = os.path.join(fw_dir, os.path.basename(dylib_path))
        shutil.copy2(dylib_path, dylib_dest)
        print(f'[+] Copied dylib to {dylib_dest}')

        dylib_install = f'@rpath/{os.path.basename(dylib_path)}'
        if not inject_dylib(binary, dylib_install):
            return False

        print(f'[*] Repacking to {output_ipa}')
        with zipfile.ZipFile(output_ipa, 'w', zipfile.ZIP_DEFLATED) as z:
            for root, _, files in os.walk(tmp):
                for file in files:
                    fp = os.path.join(root, file)
                    z.write(fp, os.path.relpath(fp, tmp))

        print(f'[+] Done: {output_ipa}')
        return True
    finally:
        shutil.rmtree(tmp)


if __name__ == '__main__':
    if len(sys.argv) < 4:
        print('usage: python3 repack_ipa.py input.ipa cheat.dylib output.ipa [--launcher]')
        sys.exit(1)
    ok = repack(sys.argv[1], sys.argv[2], sys.argv[3],
                use_launcher='--launcher' in sys.argv[4:])
    sys.exit(0 if ok else 1)
