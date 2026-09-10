#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
PE add-needed：给 EXE/DLL 增加对 tray_memprobe.dll 的导入（等价 patchelf --add-needed）。

用法:
  python inject_memprobe_pe.py <target.exe> [tray_memprobe.dll]
  python inject_memprobe_pe.py --restore <target.exe>   # 从 .bak.memprobe 还原

说明:
  - 写入新节 .trayi，追加 IMAGE_IMPORT_DESCRIPTOR，导入符号 tray_memprobe_ping
  - 启动时加载器会加载同目录（或 PATH）下的 tray_memprobe.dll，DllMain 内 IAT hook
  - 请把 tray_memprobe.dll 拷到目标 EXE 同目录，或设 PATH

依赖: 仅 Python 3 标准库（无需 pefile）
"""

from __future__ import print_function

import argparse
import os
import shutil
import struct
import sys

IMAGE_DOS_SIGNATURE = 0x5A4D
IMAGE_NT_SIGNATURE = 0x00004550
IMAGE_DIRECTORY_ENTRY_IMPORT = 1
IMAGE_SCN_CNT_INITIALIZED_DATA = 0x00000040
IMAGE_SCN_MEM_READ = 0x40000000
IMAGE_SCN_MEM_WRITE = 0x80000000


def align_up(v, a):
    return (v + a - 1) // a * a


def read_u16(data, off):
    return struct.unpack_from("<H", data, off)[0]


def read_u32(data, off):
    return struct.unpack_from("<I", data, off)[0]


def write_u32(data, off, val):
    struct.pack_into("<I", data, off, val & 0xFFFFFFFF)


def pe_offsets(data):
    if read_u16(data, 0) != IMAGE_DOS_SIGNATURE:
        raise ValueError("not a PE (MZ)")
    e_lfanew = read_u32(data, 0x3C)
    if read_u32(data, e_lfanew) != IMAGE_NT_SIGNATURE:
        raise ValueError("not a PE (NT)")
    file_header = e_lfanew + 4
    opt_off = file_header + 20
    magic = read_u16(data, opt_off)
    pe32plus = magic == 0x20B
    if pe32plus:
        dd_off = opt_off + 112
        section_align = read_u32(data, opt_off + 32)
        file_align = read_u32(data, opt_off + 36)
        size_of_image_off = opt_off + 56
        size_of_headers_off = opt_off + 60
        number_of_rva_and_sizes = read_u32(data, opt_off + 108)
    else:
        if magic != 0x10B:
            raise ValueError("unsupported OptionalHeader magic 0x%X" % magic)
        dd_off = opt_off + 96
        section_align = read_u32(data, opt_off + 32)
        file_align = read_u32(data, opt_off + 36)
        size_of_image_off = opt_off + 56
        size_of_headers_off = opt_off + 60
        number_of_rva_and_sizes = read_u32(data, opt_off + 92)
    num_sections = read_u16(data, file_header + 2)
    size_of_opt = read_u16(data, file_header + 16)
    section_table = opt_off + size_of_opt
    return {
        "e_lfanew": e_lfanew,
        "pe32plus": pe32plus,
        "dd_off": dd_off,
        "section_align": section_align,
        "file_align": file_align,
        "size_of_image_off": size_of_image_off,
        "size_of_headers_off": size_of_headers_off,
        "number_of_rva_and_sizes": number_of_rva_and_sizes,
        "num_sections": num_sections,
        "section_table": section_table,
    }


def section_info(data, meta, index):
    off = meta["section_table"] + index * 40
    name = data[off : off + 8].split(b"\0", 1)[0]
    return {
        "off": off,
        "name": name,
        "vsize": read_u32(data, off + 8),
        "va": read_u32(data, off + 12),
        "raw_size": read_u32(data, off + 16),
        "raw_ptr": read_u32(data, off + 20),
    }


def already_imports(data, meta, dll_name):
    if meta["number_of_rva_and_sizes"] <= IMAGE_DIRECTORY_ENTRY_IMPORT:
        return False
    imp_rva = read_u32(data, meta["dd_off"] + IMAGE_DIRECTORY_ENTRY_IMPORT * 8)
    if not imp_rva:
        return False
    # Map RVA -> file offset via sections
    def rva_to_off(rva):
        for i in range(meta["num_sections"]):
            s = section_info(data, meta, i)
            if s["va"] <= rva < s["va"] + max(s["vsize"], s["raw_size"]):
                return s["raw_ptr"] + (rva - s["va"])
        return None

    off = rva_to_off(imp_rva)
    if off is None:
        return False
    want = dll_name.lower().encode("ascii")
    while True:
        name_rva = read_u32(data, off + 12)
        if name_rva == 0 and read_u32(data, off) == 0 and read_u32(data, off + 16) == 0:
            break
        noff = rva_to_off(name_rva)
        if noff is not None:
            end = data.index(b"\0", noff)
            name = data[noff:end].lower()
            if name == want:
                return True
        off += 20
    return False


def add_needed(pe_path, dll_name, import_sym):
    with open(pe_path, "rb") as f:
        raw = bytearray(f.read())

    meta = pe_offsets(raw)
    if already_imports(raw, meta, dll_name):
        print("already imports %s — skip" % dll_name)
        return False

    # Header room for one more section header?
    size_of_headers = read_u32(raw, meta["size_of_headers_off"])
    new_section_header_off = meta["section_table"] + meta["num_sections"] * 40
    if new_section_header_off + 40 > size_of_headers:
        raise RuntimeError(
            "no room in PE headers for a new section (headers full). "
            "Use a larger SizeOfHeaders binary or inject at runtime."
        )

    # Extremes of existing sections
    max_va_end = 0
    max_raw_end = 0
    for i in range(meta["num_sections"]):
        s = section_info(raw, meta, i)
        max_va_end = max(max_va_end, s["va"] + align_up(max(s["vsize"], 1), meta["section_align"]))
        max_raw_end = max(max_raw_end, s["raw_ptr"] + s["raw_size"])

    # Build import blob (within new section)
    # Layout:
    #   [0] new import descriptors: old_copy... + our + null
    #   We rebuild: our descriptor first, then copy old table, then null
    # Simpler: only our descriptor + null, and chain by replacing DataDirectory to point
    # to a table that contains OUR + OLD descriptors + null.
    #
    # Pointer size
    ptr_size = 8 if meta["pe32plus"] else 4
    dll_bytes = dll_name.encode("ascii") + b"\0"
    # IMAGE_IMPORT_BY_NAME: Hint(2) + Name
    ibn = struct.pack("<H", 0) + import_sym.encode("ascii") + b"\0"
    ibn = ibn + (b"\0" if len(ibn) % 2 else b"")  # align 2

    # We'll place after copying old import descriptors
    old_imp_rva = read_u32(raw, meta["dd_off"] + IMAGE_DIRECTORY_ENTRY_IMPORT * 8)
    old_imp_size = read_u32(raw, meta["dd_off"] + IMAGE_DIRECTORY_ENTRY_IMPORT * 8 + 4)

    def rva_to_off(rva):
        for i in range(meta["num_sections"]):
            s = section_info(raw, meta, i)
            span = max(s["vsize"], s["raw_size"])
            if s["raw_ptr"] and s["va"] <= rva < s["va"] + span:
                return s["raw_ptr"] + (rva - s["va"])
        return None

    old_desc_bytes = b""
    if old_imp_rva:
        ooff = rva_to_off(old_imp_rva)
        if ooff is None:
            raise RuntimeError("cannot map old import RVA")
        # read until null descriptor
        cur = ooff
        while True:
            desc = bytes(raw[cur : cur + 20])
            if desc == b"\0" * 20:
                break
            old_desc_bytes += desc
            cur += 20
            if old_imp_size and (cur - ooff) > old_imp_size + 40:
                break

    # Section content builder with relative offsets from section VA
    parts = []
    # Reserve: ILT (2 entries), IAT (2 entries), ibn, dll name, descriptors
    # We'll compute RVAs once section_va known — build with placeholders then fix.
    # Structure of blob:
    #   offset 0: ILT: [rva_ibn, 0]
    #   then IAT: [rva_ibn, 0]
    #   then ibn
    #   then dll name
    #   then descriptors: ours + old + null

    section_va = align_up(max_va_end, meta["section_align"])
    section_raw = align_up(max(len(raw), max_raw_end), meta["file_align"])

    # Tentative layout
    off_ilt = 0
    off_iat = off_ilt + 2 * ptr_size
    off_ibn = off_iat + 2 * ptr_size
    off_dll = align_up(off_ibn + len(ibn), 4)
    off_desc = align_up(off_dll + len(dll_bytes), 4)

    rva_ilt = section_va + off_ilt
    rva_iat = section_va + off_iat
    rva_ibn = section_va + off_ibn
    rva_dll = section_va + off_dll
    rva_desc = section_va + off_desc

    blob = bytearray()
    # ILT
    if ptr_size == 8:
        blob += struct.pack("<QQ", rva_ibn, 0)
        blob += struct.pack("<QQ", rva_ibn, 0)  # IAT initially same
    else:
        blob += struct.pack("<II", rva_ibn, 0)
        blob += struct.pack("<II", rva_ibn, 0)
    assert len(blob) == off_ibn
    blob += ibn
    blob += b"\0" * (off_dll - len(blob))
    blob += dll_bytes
    blob += b"\0" * (off_desc - len(blob))

    # Our IMAGE_IMPORT_DESCRIPTOR
    # OriginalFirstThunk, TimeDateStamp, ForwarderChain, Name, FirstThunk
    our_desc = struct.pack(
        "<IIIII",
        rva_ilt,  # INT
        0,
        0,
        rva_dll,
        rva_iat,  # IAT
    )
    blob += our_desc
    blob += old_desc_bytes
    blob += b"\0" * 20  # null terminator

    vsize = len(blob)
    raw_size = align_up(vsize, meta["file_align"])
    blob += b"\0" * (raw_size - len(blob))

    # Append section data
    if len(raw) < section_raw:
        raw.extend(b"\0" * (section_raw - len(raw)))
    raw.extend(blob)

    # Write section header
    name = b".trayi\0\0"
    chars = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ | IMAGE_SCN_MEM_WRITE
    sh = name + struct.pack(
        "<IIIIIIHHI",
        vsize,
        section_va,
        raw_size,
        section_raw,
        0,
        0,
        0,
        0,
        chars,
    )
    raw[new_section_header_off : new_section_header_off + 40] = sh

    # Update NumberOfSections
    struct.pack_into("<H", raw, meta["e_lfanew"] + 4 + 2, meta["num_sections"] + 1)

    # SizeOfImage
    new_image_size = align_up(section_va + vsize, meta["section_align"])
    write_u32(raw, meta["size_of_image_off"], new_image_size)

    # Import directory -> our descriptor table
    new_imp_size = 20 + len(old_desc_bytes) + 20
    write_u32(raw, meta["dd_off"] + IMAGE_DIRECTORY_ENTRY_IMPORT * 8, rva_desc)
    write_u32(raw, meta["dd_off"] + IMAGE_DIRECTORY_ENTRY_IMPORT * 8 + 4, new_imp_size)

    bak = pe_path + ".bak.memprobe"
    if not os.path.exists(bak):
        shutil.copy2(pe_path, bak)
        print("backup: %s" % bak)

    with open(pe_path, "wb") as f:
        f.write(raw)

    print("patched: %s" % pe_path)
    print("  IMPORT += %s!%s" % (dll_name, import_sym))
    print("  section .trayi VA=0x%X raw=0x%X" % (section_va, section_raw))
    print("place %s next to the EXE (or on PATH), then run the EXE." % dll_name)
    return True


def restore(pe_path):
    bak = pe_path + ".bak.memprobe"
    if not os.path.isfile(bak):
        print("backup not found: %s" % bak, file=sys.stderr)
        return 1
    shutil.copy2(bak, pe_path)
    print("restored: %s <- %s" % (pe_path, bak))
    return 0


def find_default_dll():
    here = os.path.dirname(os.path.abspath(__file__))
    cands = [
        os.path.join(os.getcwd(), "tray_memprobe.dll"),
        os.path.join(os.getcwd(), "build", "hooks", "tray_memprobe.dll"),
        os.path.join(here, "..", "..", "build", "hooks", "tray_memprobe.dll"),
    ]
    for c in cands:
        if os.path.isfile(c):
            return os.path.normpath(c)
    return None


def main():
    ap = argparse.ArgumentParser(description="PE add-needed for tray_memprobe.dll")
    ap.add_argument("target", nargs="?", help="EXE/DLL to patch")
    ap.add_argument("dll", nargs="?", help="path to tray_memprobe.dll (for copy hint)")
    ap.add_argument("--restore", action="store_true", help="restore from .bak.memprobe")
    ap.add_argument("--dll-name", default="tray_memprobe.dll", help="import DLL basename")
    ap.add_argument("--sym", default="tray_memprobe_ping", help="imported symbol")
    args = ap.parse_args()

    if args.restore:
        if not args.target:
            ap.error("target required with --restore")
        return restore(args.target)

    if not args.target:
        ap.error("target EXE required")
    if not os.path.isfile(args.target):
        print("not found: %s" % args.target, file=sys.stderr)
        return 1

    dll_path = args.dll or find_default_dll()
    try:
        add_needed(args.target, args.dll_name, args.sym)
    except Exception as e:
        print("error: %s" % e, file=sys.stderr)
        return 1

    if dll_path:
        dest = os.path.join(os.path.dirname(os.path.abspath(args.target)), args.dll_name)
        if os.path.normcase(os.path.abspath(dll_path)) != os.path.normcase(dest):
            shutil.copy2(dll_path, dest)
            print("copied: %s -> %s" % (dll_path, dest))
    else:
        print("warning: tray_memprobe.dll not found nearby; copy it next to the EXE", file=sys.stderr)
    return 0


if __name__ == "__main__":
    sys.exit(main() or 0)
