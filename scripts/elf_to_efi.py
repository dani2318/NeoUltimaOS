#!/usr/bin/env python3
"""
elf_to_efi.py - Convert ELF64 shared object to PE32+ EFI application.
Preserves ELF section VMAs as PE RVAs so .reloc entries remain valid.
Usage: python3 elf_to_efi.py <input.so> <output.EFI>
No external dependencies.
"""

import struct
import sys

# ELF
ELFMAG     = b'\x7fELF'
SHT_NOBITS = 8

# PE
IMAGE_FILE_MACHINE_AMD64        = 0x8664
IMAGE_SUBSYSTEM_EFI_APPLICATION = 0x000A
PE_SECT_CODE     = 0x60000020
PE_SECT_DATA_RW  = 0xC0000040
PE_SECT_DATA_RO  = 0x40000040
PE_SECT_RELOC    = 0x42000040

FILE_ALIGN = 0x200
SECT_ALIGN = 0x1000

WANTED = ['.text', '.rodata', '.sdata', '.data', '.dynamic', '.dynsym',
          '.rel', '.rela', '.reloc']

def is_wanted(name):
    return name in WANTED or name.startswith('.rel.') or name.startswith('.rela.')

def align_up(v, a):
    return (v + a - 1) & ~(a - 1) if a else v

def section_chars(name):
    if name == '.text':              return PE_SECT_CODE
    if name == '.rodata':            return PE_SECT_DATA_RO
    if name in ('.data', '.sdata'): return PE_SECT_DATA_RW
    if name == '.reloc':            return PE_SECT_RELOC
    return PE_SECT_DATA_RO

def parse_elf(data):
    if data[:4] != ELFMAG:
        raise ValueError("Not an ELF file")
    if data[4] != 2:
        raise ValueError("Not ELF64")

    (e_type, e_machine, _e_version, e_entry, _e_phoff,
     e_shoff, _e_flags, _e_ehsize, _e_phentsize, _e_phnum,
     e_shentsize, e_shnum, e_shstrndx) = \
        struct.unpack_from('<HHIQQQIHHHHHH', data, 16)

    shdrs = []
    for i in range(e_shnum):
        off = e_shoff + i * e_shentsize
        sh  = struct.unpack_from('<IIQQQQIIQQ', data, off)
        shdrs.append(sh)

    strtab_sh   = shdrs[e_shstrndx]
    strtab_data = data[strtab_sh[4] : strtab_sh[4] + strtab_sh[5]]

    def sh_name(sh):
        end = strtab_data.index(b'\x00', sh[0])
        return strtab_data[sh[0]:end].decode()

    sections = {}
    for sh in shdrs:
        name    = sh_name(sh)
        sh_type = sh[1]
        sh_vma  = sh[3]
        sh_off  = sh[4]
        sh_size = sh[5]
        raw = bytes(sh_size) if sh_type == SHT_NOBITS else data[sh_off : sh_off + sh_size]
        sections[name] = {'vma': sh_vma, 'data': raw}

    return sections, e_entry

def build_pe(elf_sections, entry_va):
    # Collect wanted non-empty sections preserving ELF VMA order
    candidates = []
    seen = set()
    for want in WANTED:
        if want in elf_sections and want not in seen:
            info = elf_sections[want]
            if info['data']:
                candidates.append((want, info['vma'], info['data']))
                seen.add(want)
    for name, info in elf_sections.items():
        if name not in seen and is_wanted(name) and info['data']:
            candidates.append((name, info['vma'], info['data']))
            seen.add(name)

    if not candidates:
        raise ValueError("No sections to write")

    # Sort by ELF VMA — preserves original layout
    candidates.sort(key=lambda x: x[1])

    # Compute header size — must fit before the first section VMA
    n_sects  = len(candidates)
    hdr_raw  = 128 + 4 + 20 + 112 + 128 + 40 * n_sects
    hdr_size = align_up(hdr_raw, FILE_ALIGN)

    first_vma = candidates[0][1]
    if hdr_size > first_vma:
        raise ValueError(f"Headers ({hdr_size:#x}) overlap first section VMA ({first_vma:#x})")

    # Build section table using ELF VMAs as PE RVAs
    sects    = []
    cur_foff = hdr_size
    for name, vma, data in candidates:
        vsz  = len(data)
        rsz  = align_up(vsz, FILE_ALIGN)
        sects.append(dict(
            name  = name,
            data  = data,
            vsz   = vsz,
            rsz   = rsz,
            rva   = vma,      # ELF VMA == PE RVA
            foff  = cur_foff,
            chars = section_chars(name),
        ))
        cur_foff += rsz

    # Image size = end of last section rounded up
    last = sects[-1]
    image_size = align_up(last['rva'] + last['vsz'], SECT_ALIGN)

    # entry_va from ELF is already an RVA-equivalent since ImageBase=0
    boc     = next((s['rva'] for s in sects if s['name'] == '.text'), sects[0]['rva'])
    code_sz = sum(s['rsz'] for s in sects if s['chars'] == PE_SECT_CODE)
    data_sz = sum(s['rsz'] for s in sects if s['chars'] != PE_SECT_CODE)

    reloc_rva = reloc_sz = 0
    for s in sects:
        if s['name'] == '.reloc':
            reloc_rva, reloc_sz = s['rva'], s['vsz']
            break

    # ── assemble ─────────────────────────────────────────────────────────────
    out = bytearray(hdr_size)

    # DOS header
    struct.pack_into('<H', out, 0,  0x5A4D)
    struct.pack_into('<I', out, 60, 128)

    # PE signature
    p = 128
    struct.pack_into('<4s', out, p, b'PE\x00\x00')
    p += 4

    # COFF header
    struct.pack_into('<HHIIIHH', out, p,
        IMAGE_FILE_MACHINE_AMD64,
        n_sects,
        0, 0, 0,
        112 + 128,
        0x0022,
    )
    p += 20

    # Optional header PE32+
    struct.pack_into('<HBBIIIIIQIIHHHHHHIIIIHHQQQQII', out, p,
        0x020B,
        0, 0,
        code_sz,
        data_sz,
        0,
        entry_va,
        boc,
        0,
        SECT_ALIGN,
        FILE_ALIGN,
        0, 0,
        0, 0,
        0, 0,
        0,
        image_size,
        hdr_size,
        0,
        IMAGE_SUBSYSTEM_EFI_APPLICATION,
        0,
        0, 0,
        0, 0,
        0,
        16,
    )
    p += 112

    # Data directories
    dirs = bytearray(128)
    struct.pack_into('<II', dirs, 5 * 8, reloc_rva, reloc_sz)
    out[p:p+128] = dirs
    p += 128

    # Section headers
    for s in sects:
        name_b = s['name'].encode()[:8].ljust(8, b'\x00')
        struct.pack_into('<8sIIIIIIHHI', out, p,
            name_b, s['vsz'], s['rva'], s['rsz'], s['foff'],
            0, 0, 0, 0, s['chars'],
        )
        p += 40

    # Section data
    blob = bytearray()
    for s in sects:
        blob += s['data'] + b'\x00' * (s['rsz'] - len(s['data']))

    return bytes(out) + bytes(blob)

def main():
    if len(sys.argv) != 3:
        print(f"Usage: {sys.argv[0]} <input.so> <output.EFI>", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], 'rb') as f:
        elf_data = f.read()

    elf_sections, entry_va = parse_elf(elf_data)
    pe_data = build_pe(elf_sections, entry_va)

    with open(sys.argv[2], 'wb') as f:
        f.write(pe_data)

    pe_off = struct.unpack_from('<I', pe_data, 60)[0]
    subsys = struct.unpack_from('<H', pe_data, pe_off + 4 + 20 + 68)[0]
    entry  = struct.unpack_from('<I', pe_data, pe_off + 4 + 20 + 16)[0]

    print(f"✓ {sys.argv[2]}")
    print(f"  Size:      {len(pe_data)} bytes")
    print(f"  Subsystem: 0x{subsys:x} ({'EFI Application' if subsys == 0xA else 'UNKNOWN'})")
    print(f"  Entry RVA: 0x{entry:x}")

if __name__ == '__main__':
    main()