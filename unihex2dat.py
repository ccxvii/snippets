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
f.write(struct.pack(">III", len(table), len(cdata), len(udata)))
for low, high, w, ofs in table:
	print '\t%x %x (%d) %d %d' % (low, high, high - low + 1, w, ofs)
	f.write(struct.pack(">HHBI", low, high, w, ofs))
f.write(cdata)
f.close()

f = open("unifont.h", "w")
print >>f, "#define unifont_count %d" % len(table)
print >>f, "static const unsigned short unifont_low[%d] = {" % len(table)
for low, high, w, ofs in table: print >>f, "%d," % low
print >>f, "}"
print >>f, "static const unsigned short unifont_high[%d] = {" % len(table)
for low, high, w, ofs in table: print >>f, "%d," % high
print >>f, "}"
print >>f, "static const unsigned char unifont_width[%d] = {" % len(table)
for low, high, w, ofs in table: print >>f, "%d," % w
print >>f, "}"
print >>f, "static const unsigned int unifont_offset[%d] = {" % len(table)
for low, high, w, ofs in table: print >>f, "%d," % ofs
print >>f, "}"
print >>f, "static const unsigned char unifont_data[%d] = {" % len(udata)
i = 0
s = "\t"
for x in udata:
	if i == 7:
		print >>f, s
		s = "\t"
		i = 0
	else:
		s = s + ("0x%02x," % ord(x))
		i = i + 1
print >>f, "}"
