import sys, binascii, zlib, struct

info = []
udata = ""

for line in open(sys.argv[1]).xreadlines():
	line = line.strip()
	ucs, hex = line.split(':')
	ucs = int(ucs, 16)
	hex = binascii.a2b_hex(hex)
	w = len(hex) / 16
	ofs = len(udata)
	info.append((ucs, w, ofs))
	udata = udata + hex

table = []

low = -100
high = -100
rw = 0
rofs = 0

for ucs, w, ofs in info:
	if ucs != high + 1 or w != rw:
		if low >= 0:
			table.append((low, high, rw, rofs))
		low = ucs
		high = ucs
		rw = w
		rofs = ofs
	else:
		high = ucs

table.append((low, high, rw, rofs))

cdata = zlib.compress(udata)

print len(info), "characters"
print len(table), "ranges"
print len(udata), "bytes of uncompressed glyph data"
print len(cdata), "bytes of compressed glyph data"

f = open("unifont.dat", "wb")
f.write(struct.pack(">iii", len(table), len(cdata), len(udata)))
for low, high, w, ofs in table:
	print '\t%x %x %d %d' % (low, high, w, ofs)
	f.write(struct.pack(">iiii", low, high, w, ofs))
f.write(cdata)
f.close()

