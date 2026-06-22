#!/usr/bin/env python3
# Authored by Claude, Marcel does not know python.

# mach_o_make_weak.py — set weak_import (chained fixups) + N_WEAK_REF (nlist)
# on named undefined imports in a thin arm64 Mach-O.
#
# usage: mach_o_make_weak.py <macho> <sym1> <sym2> ...
import sys, struct

LC_DYLD_CHAINED_FIXUPS = 0x80000034
LC_SYMTAB              = 0x02
MH_MAGIC_64 = 0xfeedfacf

DYLD_CHAINED_IMPORT          = 1
DYLD_CHAINED_IMPORT_ADDEND   = 2
DYLD_CHAINED_IMPORT_ADDEND64 = 3

N_WEAK_REF = 0x0040
N_TYPE     = 0x0e
N_UNDF     = 0x00

def find_lc(buf, want):
	ncmds = struct.unpack_from('<I', buf, 16)[0]
	lc = 32
	out = []
	for _ in range(ncmds):
		cmd, cmdsize = struct.unpack_from('<II', buf, lc)
		if cmd == want:
			out.append(lc)
		lc += cmdsize
	return out

def patch_chained(buf, names, found):
	cf = find_lc(buf, LC_DYLD_CHAINED_FIXUPS)
	if not cf:
		sys.exit("no LC_DYLD_CHAINED_FIXUPS")
	dataoff, datasize = struct.unpack_from('<II', buf, cf[0]+8)
	base = dataoff
	(_ver, _starts, imports_offset, symbols_offset,
	 imports_count, imports_format, _symfmt) = struct.unpack_from('<IIIIIII', buf, base)
	imports_base = base + imports_offset
	symbols_base = base + symbols_offset

	def sym_name(name_off):
		s = symbols_base + name_off
		e = buf.index(b'\x00', s)
		return buf[s:e].decode('latin1')

	if imports_format in (DYLD_CHAINED_IMPORT, DYLD_CHAINED_IMPORT_ADDEND):
		esize = 4 if imports_format == DYLD_CHAINED_IMPORT else 8
		for i in range(imports_count):
			o = imports_base + i*esize
			val = struct.unpack_from('<I', buf, o)[0]
			name = sym_name((val >> 9) & 0x7fffff)
			if name in names:
				struct.pack_into('<I', buf, o, val | (1 << 8))  # weak_import bit 8
				found.add(name)
	elif imports_format == DYLD_CHAINED_IMPORT_ADDEND64:
		for i in range(imports_count):
			o = imports_base + i*16
			val = struct.unpack_from('<Q', buf, o)[0]
			name = sym_name((val >> 32) & 0xffffffff)
			if name in names:
				struct.pack_into('<Q', buf, o, val | (1 << 16))  # weak_import bit 16
				found.add(name)
	else:
		sys.exit(f"unknown imports_format {imports_format}")

def patch_nlist(buf, names):
	st = find_lc(buf, LC_SYMTAB)
	if not st:
		return
	symoff, nsyms, stroff, strsize = struct.unpack_from('<IIII', buf, st[0]+8)
	for i in range(nsyms):
		b = symoff + i*16
		if (buf[b+4] & N_TYPE) != N_UNDF:
			continue
		n_strx = struct.unpack_from('<I', buf, b)[0]
		s = stroff + n_strx
		e = buf.index(b'\x00', s)
		if buf[s:e].decode('latin1') in names:
			desc_off = b+6
			desc = struct.unpack_from('<H', buf, desc_off)[0]
			struct.pack_into('<H', buf, desc_off, desc | N_WEAK_REF)

def main():
	if len(sys.argv) < 3:
		sys.exit("usage: weaken_chained.py <macho> <sym1> <sym2> ...")
	path = sys.argv[1]
	names = set(sys.argv[2:])
	with open(path, 'rb') as f:
		buf = bytearray(f.read())
	if struct.unpack_from('<I', buf, 0)[0] != MH_MAGIC_64:
		sys.exit("expected thin arm64 Mach-O")
	found = set()
	patch_chained(buf, names, found)
	patch_nlist(buf, names)
	missing = names - found
	if missing: print(f"couldn't find {len(missing)} symbol(s): {' '.join(sorted(missing))}")
	with open(path, 'wb') as f:
		f.write(buf)
	print(f"weakened {len(found)} symbol(s): {' '.join(sorted(found))}")

if __name__ == '__main__':
	main()
