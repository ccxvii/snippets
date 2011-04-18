/* Truetype font parser.
 * 2004-2009 (C) Tor Andersson.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>

#define MIN(a,b) ((a) < (b) ? (a) : (b))

#define nil ((void*)0)

#define byte unsigned char
#define ushort unsigned short
#define ulong unsigned long

static int g_nglyphs;
static int g_locafmt;

void panic(char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	putc('\n', stderr);
	va_end(ap);
	exit(1);
}

#define TAG(a,b,c,d) ((ulong) (a<<24) | (b<<16) | (c<<8) | (d) )

static inline short readshort(FILE *f)
{
	byte a = getc(f);
	byte b = getc(f);
	return (a << 8) | b;
}

static inline ushort readushort(FILE *f)
{
	byte a = getc(f);
	byte b = getc(f);
	return (a << 8) | b;
}

static inline long readlong(FILE *f)
{
	byte a = getc(f);
	byte b = getc(f);
	byte c = getc(f);
	byte d = getc(f);
	return (a << 24) | (b << 16) | (c << 8) | d;
}

static inline ulong readulong(FILE *f)
{
	byte a = getc(f);
	byte b = getc(f);
	byte c = getc(f);
	byte d = getc(f);
	return (a << 24) | (b << 16) | (c << 8) | d;
}

/*
 * OpenType tables
 */

void readlangtable(FILE *f, ulong langofs)
{
	fseek(f, langofs, 0);

	(void) readushort(f);       /* lookup order. reserved. */
	ushort reqfeatidx = readushort(f);
	ushort featurecount = readushort(f);

	if (reqfeatidx != 0xFFFF)
		printf("req %u ", reqfeatidx);

	int i;
	for (i = 0; i < featurecount; i++)
	{
		ushort featidx = readushort(f);
		printf("%u ", featidx);
	}
}

void readscriptlist(FILE *f, ulong scriptlistofs)
{
	ushort scriptcount;
	int i, k;

	fseek(f, scriptlistofs, 0);

	scriptcount = readushort(f);
	byte scripttag[scriptcount][4];
	ushort scriptofs[scriptcount];

	for (i = 0; i < scriptcount; i++)
	{
		fread(scripttag[i], 1, 4, f);
		scriptofs[i] = readushort(f);
	}

	for (i = 0; i < scriptcount; i++)
	{
		printf("    script %4.4s\n    {\n", scripttag[i]);

		fseek(f, scriptlistofs + scriptofs[i], 0);

		ushort defofs = readushort(f);
		ushort langcount = readushort(f);
		byte langtag[langcount][4];
		ushort langofs[langcount];

		for (k = 0; k < langcount; k++) {
			fread(langtag[k], 1, 4, f);
			langofs[k] = readushort(f);
		}

		if (defofs) {
			printf("\tlang default [ ");
			readlangtable(f, scriptlistofs + scriptofs[i] + defofs);
			printf("]\n");
		}

		for (k = 0; k < langcount; k++) {
			printf("\tlang %4.4s [ ", langtag[k]);
			readlangtable(f, scriptlistofs + scriptofs[i] + langofs[k]);
			printf("]\n");
		}

		printf("    }\n");
	}
}

void readfeature(FILE *f, ulong ofs)
{
	fseek(f, ofs, 0);
	(void) readushort(f); /* feature params. reserved. */
	ushort count = readushort(f);
	int i;
	for (i = 0; i < count; i++)
	{
		ushort lookup = readushort(f);
		printf("%u ", lookup);
	}
}

void readfeaturelist(FILE *f, ulong featlistofs)
{
	ushort featcount;
	int i;

	fseek(f, featlistofs, 0);

	featcount = readushort(f);
	for (i = 0; i < featcount; i++)
	{
		byte tag[4];
		fread(tag, 1, 4, f);
		ulong featofs = readushort(f);
		ulong tmpofs = ftell(f);
		printf("    feat %4.4s [ ", tag);
		readfeature(f, featlistofs + featofs);
		printf("] %% %d\n", i);
		fseek(f, tmpofs, 0);
	}
}

enum { GPOS, GSUB };

void readlookup(FILE *f, ulong lookupofs, int what)
{
	fseek(f, lookupofs, 0);

	ushort type = readushort(f);
	printf("\ttype = ");
	if (what == GPOS)
	{
		switch (type)
		{
		case 1: puts("Single adjustment"); break;
		case 2: puts("Pair adjustment"); break;
		case 3: puts("Cursive attachment"); break;
		case 4: puts("MarkToBase attachment"); break;
		case 5: puts("MarkToLigature attachment"); break;
		case 6: puts("MarkToMark attachment"); break;
		case 7: puts("Context positioning"); break;
		case 8: puts("Chained Context positioning"); break;
		case 9: puts("Extension positioning"); break;
		default: puts("<unknown>"); break;
		}
	}
	else if (what == GSUB)
	{
		switch (type)
		{
		case 1: puts("Single"); break;
		case 2: puts("Multiple"); break;
		case 3: puts("Alternate"); break;
		case 4: puts("Ligature"); break;
		case 5: puts("Context"); break;
		case 6: puts("Chaining Context"); break;
		case 7: puts("Extension Substitution"); break;
		case 8: puts("Reverse chaining context single"); break;
		default: puts("<unknown>"); break;
		}
	}
	else printf("%u\n", type);

	ushort flag = readushort(f);
	printf("\tflag = [ ");
	if (flag & 0x0001) printf("RTL ");
	if (flag & 0x0002) printf("IgnBase ");
	if (flag & 0x0004) printf("IgnLiga ");
	if (flag & 0x0008) printf("IgnMark ");
	if (flag & 0xFF00) printf("MarkAttType ");
	printf("]\n");

	ushort subcount = readushort(f);
	printf("\tsubtables (%d)\n", subcount);
}

void readlookuplist(FILE *f, ulong lookuplistofs, int what)
{
	int i;

	fseek(f, lookuplistofs, 0);

	ushort count = readushort(f);
	ushort lookup[count];

	for (i = 0; i < count; i++)
		lookup[i] = readushort(f);
	for (i = 0; i < count; i++)
	{
		printf("    look %d\n    {\n", i);
		readlookup(f, lookuplistofs + lookup[i], what);
		printf("    }\n\n");
	}
}

void readGSUB(FILE *f, ulong ofs)
{
	fseek(f, ofs, 0);

	(void) readulong(f); /* version */
	ushort scriptlist = readushort(f);
	ushort featurelist = readushort(f);
	ushort lookuplist = readushort(f);

	printf("GSUB\n{\n");

	readscriptlist(f, ofs + scriptlist);
	printf("\n");
	readfeaturelist(f, ofs + featurelist);
	printf("\n");
	readlookuplist(f, ofs + lookuplist, GSUB);

	printf("}\n\n");
}

void readGPOS(FILE *f, ulong ofs)
{
	fseek(f, ofs, 0);

	(void) readulong(f); /* version */
	ushort scriptlist = readushort(f);
	ushort featurelist = readushort(f);
	ushort lookuplist = readushort(f);

	printf("GPOS\n{\n");

	readscriptlist(f, ofs + scriptlist);
	printf("\n");
	readfeaturelist(f, ofs + featurelist);
	printf("\n");
	readlookuplist(f, ofs + lookuplist, GPOS);

	printf("}\n\n");
}

/*
 * CMAP encoding tables
 */

void readcmap0(FILE *f)
{
	int i;

	ushort length;
	ushort language;

	length = readushort(f);
	language = readushort(f);

	printf("\tformat = 0\n");
	printf("\tlanguage = %u\n", language);
	printf("\tgids = [\n");

	for (i = 0; i < 256; i++)
	{
		byte gid = getc(f);
		if (i % 16 == 0) printf("\t  ");
		printf("%02x", gid);
		putchar(i % 16 == 15 ? '\n' : ' ');
	}

	printf("\t]\n");
}

void readcmap2(FILE *f)
{
	int nsubheaders;
	int ngids;
	int i;

	ushort length;
	ushort language;
	ushort subheadkeys[256];

	length = readushort(f);
	language = readushort(f);

	printf("\tformat = 2\n");
	printf("\tlanguage = %u\n", language);
	printf("\tsubheadkeys = [\n");

	nsubheaders = 0;
	for (i = 0; i < 256; i++) {
		ushort key = readushort(f) / 8;
		subheadkeys[i] = key;
		if (nsubheaders < key)
			nsubheaders = key;
		if (i % 16 == 0) printf("\t  ");
		printf("%u", key);
		putchar(i % 16 == 15 ? '\n' : ' ');
	}
	printf("\t]\n");

	ngids = (length / 2) - ((256 + 3) + nsubheaders * 4);

	printf("\tsubhead = [\n");

	for (i = 0; i < nsubheaders; i++)
	{
		ushort first = readushort(f);
		ushort count = readushort(f);
		short delta = readshort(f);
		ushort rangeofs = readushort(f) - (nsubheaders - i) * 8 - 2;
		printf("\t  <%04x> %5u %6d %5u\n",
				first, count, delta, rangeofs);
	}

	printf("\t]\n");

	if (ngids)
	{
		printf("\tgids = [\n");
		for (i = 0; i < ngids; i++)
		{
			ushort gid = readushort(f);
			if (i % 12 == 0) printf("\t  ");
			printf("%04x", gid);
			putchar(i % 12 == 11 ? '\n' : ' ');
		}
		if (i % 12 != 0) putchar('\n');
		printf("\t]\n");
	}
}

void readcmap4(FILE *f)
{
	int segcount;
	int i;

	ushort length;
	ushort language;
	ushort segcountx2;

	length = readushort(f);
	language = readushort(f);
	segcountx2 = readushort(f);

	(void) readushort(f);       /* searchRange */
	(void) readushort(f);       /* entrySelector */
	(void) readushort(f);       /* rangeShift */

	segcount = segcountx2 / 2;

	printf("\tformat = 4\n");
	printf("\tlanguage = %u\n", language);

	ushort endcode[segcount];
	ushort startcode[segcount];
	short iddelta[segcount];
	ushort idrangeofs[segcount];

	for (i = 0; i < segcount; i++)
		endcode[i] = readushort(f);

	(void) readushort(f);       /* reservedPad */

	for (i = 0; i < segcount; i++)
		startcode[i] = readushort(f);
	for (i = 0; i < segcount; i++)
		iddelta[i] = readshort(f);
	for (i = 0; i < segcount; i++)
		idrangeofs[i] = readushort(f);

	printf("\tranges = [\n");

	for (i = 0; i < segcount; i++) {
		printf("\t  <%04x> <%04x> %6d %5u\n",
				startcode[i], endcode[i], iddelta[i], idrangeofs[i]);
	}

	printf("\t]\n");

	ushort gidlen = length - (4 * segcount * 2 + 16);
	if (gidlen)
	{
		printf("\tgids = [\n");
		for (i = 0; i < gidlen; i++)
		{
			ushort gid = readushort(f);
			if (i % 12 == 0) printf("\t  ");
			printf("%04x", gid);
			putchar(i % 12 == 11 ? '\n' : ' ');
		}
		if (i % 12 != 0) putchar('\n');
		printf("\t]\n");
	}
}

void readcmap6(FILE *f)
{
	int i;

	ushort length;
	ushort language;
	ushort first;
	ushort count;

	length = readushort(f);
	language = readushort(f);

	printf("\tformat = 6\n");
	printf("\tlanguage = %u\n", language);

	first = readushort(f);
	count = readushort(f);

	printf("\tfirst = <%04x>\n", first);
	printf("\tgids = [\n");

	for (i = 0; i < count; i++)
	{
		printf("\tgids = [\n");
		for (i = 0; i < count; i++)
		{
			ushort gid = readushort(f);
			if (i % 12 == 0) printf("\t  ");
			printf("%04x", gid);
			putchar(i % 12 == 11 ? '\n' : ' ');
		}
		if (i % 12 != 0) putchar('\n');
		printf("\t]\n");
	}
}

void readcmap8(FILE *f)
{
	int i;

	ulong length;
	ulong language;
	byte is32[65536 / 8];
	ulong ngroups;

	ulong startchar;
	ulong endchar;
	ulong startglyph;

	length = readulong(f);
	language = readulong(f);
	fread(is32, 1, 65536 / 8, f);
	ngroups = readulong(f);

	printf("\tformat = 8\n");
	printf("\tlanguage = %lu\n", language);

	printf("\tngroups %lu\n", ngroups);

	for (i = 0; i < ngroups; i++)
	{
		startchar = readulong(f);
		endchar = readulong(f);
		startglyph = readulong(f);
	}
}

void readcmap10(FILE *f)
{
	int i;

	ulong length;
	ulong language;
	ulong startchar;
	ulong count;

	length = readulong(f);
	language = readulong(f);
	startchar = readulong(f);
	count = readulong(f);

	printf("\tformat = 10\n");
	printf("\tlanguage = %lu\n", language);

	for (i = 0; i < count; i++)
	{
		(void) readushort(f);
	}
}

void readcmap12(FILE *f)
{
	int i;

	ulong length;
	ulong language;
	ulong ngroups;

	ulong startchar;
	ulong endchar;
	ulong startglyph;

	length = readulong(f);
	language = readulong(f);
	ngroups = readulong(f);

	printf("\tformat = 12\n");
	printf("\tlanguage = %lu\n", language);

	printf("\tngroups %lu\n", ngroups);

	for (i = 0; i < ngroups; i++)
	{
		startchar = readulong(f);
		endchar = readulong(f);
		startglyph = readulong(f);
		printf("\t\t<%lx> <%lx> %lu\n", startchar, endchar, startglyph);
	}
}

void readcmap(FILE *f, ulong cmapofs)
{
	int i;

	ushort version;
	ushort nsubtables;

	ushort format;
	ushort subformat;

	fseek(f, cmapofs, 0);

	version = readushort(f);
	printf("cmap\n{\n");

	nsubtables = readushort(f);

	ulong offsets[nsubtables];
	ushort pids[nsubtables];
	ushort eids[nsubtables];

	for (i = 0; i < nsubtables; i++)
	{
		pids[i] = readushort(f);
		eids[i] = readushort(f);
		offsets[i] = readulong(f);
	}

	for (i = 0; i < nsubtables; i++)
	{
		printf("    platform %u encoding %u\n", pids[i], eids[i]);
		printf("    {\n");

		fseek(f, cmapofs + offsets[i], 0);

		format = readushort(f);
		if (format > 6)
			subformat = readushort(f);
		else
			subformat = 0;

		switch (format)
		{
		case 0: readcmap0(f); break;
		case 2: readcmap2(f); break;
		case 4: readcmap4(f); break;
		case 6: readcmap6(f); break;
		case 8: readcmap8(f); break;
		case 10: readcmap10(f); break;
		case 12: readcmap12(f); break;
		default: panic("unknown cmap format: %u\n", format);
		}

		printf("    }\n\n");
	}

	printf("}\n\n");
}

/*
 * Misc TrueType headers and metrics and icky bits.
 */

void readhead(FILE *f, ulong headofs)
{
	fseek(f, headofs, 0);

	ulong tableversion = readulong(f);
	ulong fontrevision = readulong(f);
	//ulong checksumadj = readulong(f);
	(void) readulong(f);
	ulong magicnumber = readulong(f);
	ushort flags = readushort(f);
	ushort unitsperem = readushort(f);
	//struct datetime created = readdatetime(f);
	(void) readulong(f);
	(void) readulong(f);
	//struct datetime modified = readdatetime(f);
	(void) readulong(f);
	(void) readulong(f);
	//short xmin = readshort(f);
	//short ymin = readshort(f);
	//short xmax = readshort(f);
	//short ymax = readshort(f);
	(void) readshort(f);
	(void) readshort(f);
	(void) readshort(f);
	(void) readshort(f);
	ushort macstyle = readushort(f);
	//ushort lowestrecppem = readushort(f);
	(void) readushort(f);
	short fontdirhint = readshort(f);
	short indextolocfmt = readshort(f);
	//short glyphdatafmt = readshort(f);
	(void) readshort(f);

	if (tableversion != 0x00010000)
		panic("unknown version of head table: 0x%08x\n", tableversion);
	if (magicnumber != 0x5F0F3CF5)
		panic("bad voodoo: 0x%08x\n", magicnumber);

	printf("head\n{\n");

	printf("    font-revision = 0x%08lx\n", fontrevision);
	printf("    units-per-em = %u\n", unitsperem);

	printf("    bidi = ");
	if (fontdirhint == 0) puts("fully-mixed-directional-glyphs");
	else if (fontdirhint == 1) puts("strongly-ltr");
	else if (fontdirhint == 2) puts("ltr-with-neutrals");
	else if (fontdirhint == -1) puts("strongly-rtl");
	else if (fontdirhint == -2) puts("rtl-with-neutrals");
	else puts("unknown");

	printf("    style = [ ");
	if (macstyle & 0x0001) printf("bold ");
	if (macstyle & 0x0002) printf("italic ");
	if (macstyle & 0x0004) printf("underline ");
	if (macstyle & 0x0008) printf("outline ");
	if (macstyle & 0x0010) printf("shadow ");
	if (macstyle & 0x0020) printf("condensed ");
	if (macstyle & 0x0040) printf("extended ");
	printf("]\n");

	printf("    flags = [\n");
	if (flags & 0x0001) puts("\tbaseline at y=0");
	if (flags & 0x0002) puts("\txmin is lsb");
	if (flags & 0x0004) puts("\tinstructions may depend on point size");
	if (flags & 0x0008) puts("\tuse integer scaling instead of fractional");
	if (flags & 0x0010) puts("\tinstructions may alter advance width");

	/* apple flags */
	if (flags & 0x0020) puts("\tapple: vertical baseline x=0");
	if (flags & 0x0080) puts("\tapple: requires layout");
	if (flags & 0x0100) puts("\tapple: has gx morph effects by default");
	if (flags & 0x0200) puts("\tapple: has strong rtl glyphs");
	if (flags & 0x0400) puts("\tapple: has indic rearrangement");

	/* adobe flags */
	if (flags & 0x0800) puts("\tadobe: lossless fontdata");
	if (flags & 0x1000) puts("\tadobe: converted font");
	if (flags & 0x2000) puts("\tadobe: optimised for cleartype");
	printf("    ]\n");

	printf("    locaformat = %s\n", indextolocfmt == 0 ? "short" : "long");

	printf("}\n\n");

	g_locafmt = indextolocfmt;
}

void readmaxp(FILE *f, ulong maxpofs)
{
	ulong version;
	ushort nglyphs;
	ushort maxpoints;
	ushort maxcontours;
	ushort maxcpoints;
	ushort maxccontours;
	/* ... */

	fseek(f, maxpofs, 0);

	printf("maxp\n{\n");

	version = readulong(f);
	if (version == 0x00005000) {
		nglyphs = readushort(f);
		printf("    numglyphs = %u\n", nglyphs);
	}
	else if (version == 0x00010000) {
		nglyphs = readushort(f);
		maxpoints = readushort(f);
		maxcontours = readushort(f);
		maxcpoints = readushort(f);
		maxccontours = readushort(f);
		printf("    numglyphs = %u\n", nglyphs);
		printf("    maxpoints = %u\n", maxpoints);
		printf("    maxcontours = %u\n", maxcontours);
		printf("    maxcompositepoints = %u\n", maxcpoints);
		printf("    maxcompositecontours = %u\n", maxccontours);
	}
	else {
		panic("unknown maxp version: 0x%08x\n", version);
		nglyphs = 0;
	}

	printf("}\n\n");

	g_nglyphs = nglyphs;
}

void readname(FILE *f, ulong nameofs)
{
	int i, k;

	ushort format;
	ushort count;
	ushort strofs;

	ushort pid;
	ushort eid;
	ushort langid;
	ushort nameid;
	ushort length;
	ushort offset;

	fseek(f, nameofs, 0);

	format = readushort(f);
	count = readushort(f);
	strofs = readushort(f);

	printf("name\n{\n");

	for (i = 0; i < count; i++)
	{
		pid = readushort(f);
		eid = readushort(f);
		langid = readushort(f);
		nameid = readushort(f);
		length = readushort(f);
		offset = readushort(f);

		if ((pid == 1 && eid == 0 && langid == 0) ||
			(pid == 3 && eid == 1 && langid == 0x0409))
		{
			char s[256];
			switch (nameid)
			{
			case  0: sprintf(s, "copyright"); break;
			case  1: sprintf(s, "family"); break;
			case  2: sprintf(s, "subfamily"); break;
			case  3: sprintf(s, "fontid"); break;
			case  4: sprintf(s, "fullname"); break;
			case  5: sprintf(s, "version"); break;
			case  6: sprintf(s, "psname"); break;
			case  7: sprintf(s, "trademark"); break;
			case  8: sprintf(s, "manufacturer"); break;
			case  9: sprintf(s, "designer"); break;
			case 10: sprintf(s, "description"); break;
			case 11: sprintf(s, "vendor-url"); break;
			case 12: sprintf(s, "designer-url"); break;
			case 13: sprintf(s, "license"); break;
			case 14: sprintf(s, "license-url"); break;
			case 15: sprintf(s, "<reserved>"); break;
			case 16: sprintf(s, "preferred-family"); break;
			case 17: sprintf(s, "preferred-subfamily"); break;
			case 18: sprintf(s, "compatible-full-name"); break;
			case 19: sprintf(s, "sampletext"); break;
			case 20: sprintf(s, "cid-findfont-name"); break;
			default: sprintf(s, "<unknown-%d>", nameid); break;
			}

			byte string[length];
			ulong oldofs = ftell(f);

			fseek(f, nameofs + strofs + offset, 0);
			fread(string, 1, length, f);
			printf("    %-12s = \"", s);
			if (pid == 3)
				for (k = 1; k < length; k += 2)
					putchar(string[k]);
			else
				fwrite(string, 1, length, stdout);

			printf("\"\n");

			fseek(f, oldofs, 0);
		}
	}

	printf("}\n\n");
}

void readhheahmtx(FILE *f, ulong hheaofs, ulong hmtxofs, char hv)
{
	int i;

	fseek(f, hheaofs, 0);

	ulong version = readulong(f);
	short ascender = readshort(f);
	short descender = readshort(f);
	short linegap = readshort(f);
	ushort maxadvw; maxadvw = readushort(f);
	short minlsb; minlsb = readshort(f);
	short minrsb; minrsb = readshort(f);
	short xmaxext; xmaxext = readshort(f);
	short caretrise = readshort(f);
	short caretrun = readshort(f);
	short caretoffset = readshort(f);
	(void) readshort(f);
	(void) readshort(f);
	(void) readshort(f);
	(void) readshort(f);
	short metricfmt; metricfmt = readshort(f);
	ushort nmetrics = readushort(f);

	printf("%chea\n{\n", hv);
	printf("    ascender = %d\n", ascender);
	printf("    descender = %d\n", descender);
	printf("    linegap = %d\n", linegap);
	printf("    caret = %d / %d + %d\n", caretrise, caretrun, caretoffset);
	printf("}\n\n");

	fseek(f, hmtxofs, 0);

	printf("%cmtx\n{\n", hv);

	for (i = 0; i < nmetrics; i++)
	{
		ushort advw = readushort(f);
		short lsb = readshort(f);
		printf("  %5u %6d\n", advw, lsb);
	}

	for ( ; i < g_nglyphs; i++)
	{
		short lsb = readshort(f);
		printf("  %6d\n", lsb);
	}

	printf("}\n\n");
}

void readpost(FILE *f, ulong postofs)
{
	int i;

	fseek(f, postofs, 0);

	long format = readlong(f); // Fixed
	long italicangle = readlong(f); // Fixed
	short underlineposition = readshort(f); // FWord
	short underlinethickness = readshort(f); // FWord
	ulong isfixedpitch = readulong(f);
	readulong(f); // minMemType42
	readulong(f); // maxMemType42
	readulong(f); // minMemType1
	readulong(f); // maxMemType1

	printf("post\n{\n");

	printf("    italic angle = %g\n", italicangle / 65536.0);
	printf("    underline position = %d\n", underlineposition);
	printf("    underline thickness = %d\n", underlinethickness);
	printf("    fixed pitch = %d\n", isfixedpitch);

	switch (format)
	{
	case 1 << 16:
		printf("    format 1 (standard macintosh ordering)\n");
		break;
	case 2 << 16:
		printf("    format 2\n");
		printf("    {\n");
		ushort numberofglyphs = readushort(f);
		int numberofnewglyphs = 0;
		printf("        number of glyphs = %d\n", numberofglyphs);
		for (i = 0; i < numberofglyphs; i++)
		{
			int nameidx = readushort(f);
			printf("        glyph %d = name %d\n", i, nameidx);
			if (nameidx > 258)
			{
				nameidx -= 258;
				if (nameidx > numberofnewglyphs)
					numberofnewglyphs = nameidx;
			}
		}
		for (i = 0; i < numberofnewglyphs; i++)
		{
			int n = getc(f);
			printf("        name %d = (", i + 258);
			while (n--)
				putchar(getc(f));
			printf(")\n");
		}
		printf("    }\n");
		break;
	default:
		printf("    format %g\n", format / 65536.0);
	}

	printf("}\n\n");
}

/*
 * Outlines
 */

ulong readglyfofs(FILE *f, ulong locaofs, int gid)
{
	if (g_locafmt == 0) {
		fseek(f, locaofs + gid * 2, 0);
		return readushort(f) * 2;
	}
	else {
		fseek(f, locaofs + gid * 4, 0);
		return readulong(f);
	}
}

ulong readglyflen(FILE *f, ulong locaofs, int gid)
{
	ulong a, b;
	if (g_locafmt == 0) {
		fseek(f, locaofs + gid * 2, 0);
		a = readushort(f) * 2;
		b = readushort(f) * 2;
	}
	else {
		fseek(f, locaofs + gid * 4, 0);
		a = readulong(f);
		b = readulong(f);
	}
	return b - a;
}

void readoneglyf(FILE *f, ulong ofs, ulong len)
{
	short ncont;
	short xmin, ymin, xmax, ymax;
	int i;

	fseek(f, ofs, 0);

	ncont = readshort(f);
	xmin = readshort(f);
	ymin = readshort(f);
	xmax = readshort(f);
	ymax = readshort(f);

	/* simple glyph */
	if (ncont > 0)
	{
		ushort endpts[ncont];
		for (i = 0; i < ncont; i++)
			endpts[i] = readushort(f);

		ushort instlen = readushort(f);
		byte inst[instlen];
		for (i = 0; i < instlen; i++)
			inst[i] = getc(f);

		ushort npts = endpts[ncont - 1] + 1;
		byte flags[npts];
		byte *fp = flags;

		while (fp < flags + npts)
		{
			byte c;
			*fp++ = c = getc(f);
			if (c & 8) { /* repeat bit set */
				byte count = getc(f);
				while (count --)
					*fp++ = c;
			}
		}

		short xs[npts];
		short oldx = 0;
		for (i = 0; i < npts; i++)
		{
			int xshort = flags[i] & 2;
			int xsame = flags[i] & 16;
			if (xshort) {
				if (xsame)
					xs[i] = getc(f);
				else
					xs[i] = - getc(f);
			}
			else {
				if (xsame)
					xs[i] = oldx;
				else
					xs[i] = readshort(f);
			}
		}

		short ys[npts];
		short oldy = 0;
		for (i = 0; i < npts; i++)
		{
			int yshort = flags[i] & 4;
			int ysame = flags[i] & 32;
			if (yshort) {
				if (ysame)
					ys[i] = getc(f);
				else
					ys[i] = - getc(f);
			}
			else {
				if (ysame)
					ys[i] = oldy;
				else
					ys[i] = readshort(f);
			}
		}

		int k = 0;
		if (npts) printf("\tcontour {\n");
		for (i = 0; i < npts; i++) {
			printf("\t  %6d %6d %s\n", xs[i], ys[i], flags[i] & 1 ? "on" : "off");
			if (i == endpts[k]) {
				printf("\t}\n");
				if (k < ncont - 1) printf("\tcontour {\n");
				k ++;
			}
		}
	}

	/* compound glyph */
	else
	{

#define ARG_1_AND_2_ARE_WORDS           (1 << 0)
#define ARGS_ARE_XY_VALUES                      (1 << 1)
#define ROUND_XY_TO_GRID                        (1 << 2)
#define WE_HAVE_A_SCALE                         (1 << 3)
#define RESERVED                                        (1 << 4)
#define MORE_COMPONENTS                         (1 << 5)
#define WE_HAVE_AN_X_AND_Y_SCALE        (1 << 6)
#define WE_HAVE_A_TWO_BY_TWO            (1 << 7)
#define WE_HAVE_INSTRUCTIONS            (1 << 8)
#define USE_MY_METRICS                          (1 << 9)
#define OVERLAP_COMPOUND                        (1 << 10)
#define SCALED_COMPONENT_OFFSET         (1 << 11)
#define UNSCALED_COMPONENT_OFFSET       (1 << 12)

		ushort flags;
		ushort gid;
		short arg1, arg2;
		short xx, xy, yx, yy;   /* 2.14 */
		do
		{
			flags = readushort(f);
			gid = readushort(f);

			if (flags & ARG_1_AND_2_ARE_WORDS) {
				arg1 = readshort(f);
				arg2 = readshort(f);
			}
			else {
				arg1 = getc(f);
				arg2 = getc(f);
			}

			xx = yy = 1 << 14;  /* 1.0 in f2dot14 */
			xy = yx = 0;

			if (flags & WE_HAVE_A_SCALE) {
				xx = readshort(f);
				yy = xx;
			}
			else if (flags & WE_HAVE_AN_X_AND_Y_SCALE)
			{
				xx = readshort(f);
				yy = readshort(f);
			}
			else if (flags & WE_HAVE_A_TWO_BY_TWO)
			{
				xx = readshort(f);
				yx = readshort(f);
				xy = readshort(f);
				yy = readshort(f);
			}

			printf("\tsubr %5u %6d %6d (%s) [%0.2f %0.2f %0.2f %0.2f]\n", gid,
					arg1, arg2, flags & ARGS_ARE_XY_VALUES ? "xy" : "pt",
					xx / 16384.0, yx / 16384.0, xy / 16384.0, yy / 16384.0);
		}
		while (flags & MORE_COMPONENTS);
	}
}

void readglyf(FILE *f, ulong glyfofs, ulong locaofs)
{
	ulong ofs;
	ulong len;
	int i;

	printf("glyf\n{\n");

	for (i = 0; i < g_nglyphs; i++)
	{
		ofs = readglyfofs(f, locaofs, i);
		len = readglyflen(f, locaofs, i);
		printf("    glyf %d\n    {\n", i);
		readoneglyf(f, glyfofs + ofs, len);
		printf("    }\n\n");
	}

	printf("}\n\n");
}

void dumptable(FILE *f, ulong ofs, ulong len, char *name)
{
	FILE *o;
	byte buf[4096];
	int n;

	o = fopen(name, "w");

	fseek(f, ofs, 0);

	while (1)
	{
		n = fread(buf, 1, MIN(4096, len), f);
		if (n < 0)
			break;
		if (n == 0)
			break;

		fwrite(buf, 1, n, o);

		len -= n;
	}

	fclose(o);
}

/*
 * Main
 */

void readfontdir(FILE *f)
{
	ulong cmapofs = 0;
	ulong headofs = 0;
	ulong maxpofs = 0;
	ulong nameofs = 0;
	ulong hheaofs = 0;
	ulong hmtxofs = 0;
	ulong vheaofs = 0;
	ulong vmtxofs = 0;
	ulong locaofs = 0;
	ulong glyfofs = 0;
	ulong GSUBofs = 0;
	ulong GPOSofs = 0;
	ulong CFF_ofs = 0;
	ulong CFF_len = 0;
	ulong postofs = 0;
	ulong postlen = 0;
	int i;

	ulong version;
	ushort numtables;

	ulong tag;
	ulong checksum;
	ulong offset;
	ulong length;

	printf("truetype\n{\n");

	version = readulong(f);
	if (version == 0x00010000)
		printf("    version = 0x00010000\n");
	else if (version == TAG('t','r','u','e'))
		printf("    version = 'true'\n");
	else if (version == TAG('O','T','T','O'))
		printf("    version = 'OTTO'\n");
	else if (version == TAG('t','y','p','1'))
		printf("    version = 'typ1'\n");
	else
		panic("unknown version: 0x%08x\n", version);

	numtables = readushort(f);

	(void) readushort(f); /* searchRange */
	(void) readushort(f); /* entrySelector */
	(void) readushort(f); /* rangeShift */

	for (i = 0; i < numtables; i++)
	{
		tag = readulong(f);
		checksum = readulong(f);
		offset = readulong(f);
		length = readulong(f);

		printf("    table '%c%c%c%c' %lu %lu\n",
				(byte)(tag >> 24), (byte)(tag >> 16), (byte)(tag >> 8), (byte)tag,
				offset, length);

		if (tag == TAG('c','m','a','p'))
			cmapofs = offset;
		if (tag == TAG('h','e','a','d'))
			headofs = offset;
		if (tag == TAG('m','a','x','p'))
			maxpofs = offset;
		if (tag == TAG('n','a','m','e'))
			nameofs = offset;
		if (tag == TAG('h','h','e','a'))
			hheaofs = offset;
		if (tag == TAG('h','m','t','x'))
			hmtxofs = offset;
		if (tag == TAG('v','h','e','a'))
			vheaofs = offset;
		if (tag == TAG('v','m','t','x'))
			vmtxofs = offset;
		if (tag == TAG('g','l','y','f'))
			glyfofs = offset;
		if (tag == TAG('l','o','c','a'))
			locaofs = offset;

		if (tag == TAG('G','S','U','B'))
			GSUBofs = offset;
		if (tag == TAG('G','P','O','S'))
			GPOSofs = offset;

		if (tag == TAG('p','o','s','t'))
		{
			postofs = offset;
			postlen = length;
		}

		if (tag == TAG('C','F','F',' '))
		{
			CFF_ofs = offset;
			CFF_len = length;
		}
	}

	printf("}\n\n");

	if (headofs) readhead(f, headofs);
	if (nameofs) readname(f, nameofs);
	if (maxpofs) readmaxp(f, maxpofs);
	if (cmapofs) readcmap(f, cmapofs);

	if (GSUBofs) readGSUB(f, GSUBofs);
	if (GPOSofs) readGPOS(f, GPOSofs);

	if (hheaofs && hmtxofs)
		readhheahmtx(f, hheaofs, hmtxofs, 'h');

	if (vheaofs && vmtxofs)
		readhheahmtx(f, vheaofs, vmtxofs, 'v');

	if (postofs)
		readpost(f, postofs);

	if (CFF_ofs)
		dumptable(f, CFF_ofs, CFF_len, "out.cff");

	if (locaofs && glyfofs)
		readglyf(f, glyfofs, locaofs);
}

int main(int argc, char **argv)
{
	if (argc < 2)
		panic("must give filename");

	FILE *f = fopen(argv[1], "r");
	if (!f)
		panic("fopen failed");

	readfontdir(f);

	fclose(f);

	return 0;
}
