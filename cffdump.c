/* CFF font parser.
 * 2006 (C) Tor Andersson.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include <errno.h>
#include <math.h> // for sqrt

static int cffpos; /* offset in file of cff (for opentypes) */
static int cfflen;

static int fontcount;

static int gsubrcount;
static unsigned char **gsubrdata;

static struct
{
    int charset;
    int encoding;
    int charstrings;
    int privatelen;
    int privateofs;
    int fdarray;
    int fdselect;
    int subrs; /* from private */
    int glyphcount; /* from charstrings */

    int subrcount; /* from subrs */
    unsigned char **subrdata;
} fontinfo[256];

void die(char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    putc('\n', stderr);
    va_end(ap);
    exit(1);
}

typedef unsigned char byte;

enum { INTEGER, REAL };

struct number { int type; int ival; float rval; };

static inline int read8(FILE *f)
{
    return getc(f);
}

static inline int read16(FILE *f)
{
    int a = getc(f);
    int b = getc(f);
    return (a << 8) | b;
}

static inline int read24(FILE *f)
{
    int a = getc(f);
    int b = getc(f);
    int c = getc(f);
    return (a << 16) | (b << 8) | c;
}

static inline int read32(FILE *f)
{
    int a = getc(f);
    int b = getc(f);
    int c = getc(f);
    int d = getc(f);
    return (a << 24) | (b << 16) | (c << 8) | d;
}

static inline int readoffset(FILE *f, int offsize)
{
    if (offsize == 1) return read8(f);
    if (offsize == 2) return read16(f);
    if (offsize == 3) return read24(f);
    if (offsize == 4) return read32(f);
    die("invalid offset size");
    return EOF;
}

static inline void seekpos(FILE *f, int pos)
{
    fseek(f, cffpos + pos, 0);
}

static inline int tellpos(FILE *f)
{
    return ftell(f) - cffpos;
}

/* XXX not tested */
static float readreal(FILE *f)
{
    char buf[64];
    char *p = buf;

    /* b0 was 30 */

    while (p < buf + (sizeof buf) - 3)
    {
        int b, n;

        b = read8(f);

        n = (b >> 4) & 0xf;
        if (n < 0xA) { *p++ = n + '0'; }
        else if (n == 0xA) { *p++ = '.'; }
        else if (n == 0xB) { *p++ = 'E'; }
        else if (n == 0xC) { *p++ = 'E'; *p++ = '-'; }
        else if (n == 0xE) { *p++ = '-'; }
        else if (n == 0xF) { break; }

        n = b & 0xf;
        if (n < 0xA) { *p++ = n + '0'; }
        else if (n == 0xA) { *p++ = '.'; }
        else if (n == 0xB) { *p++ = 'E'; }
        else if (n == 0xC) { *p++ = 'E'; *p++ = '-'; }
        else if (n == 0xE) { *p++ = '-'; }
        else if (n == 0xF) { break; }
    }

    *p = 0;

    return atof(buf);
}

static int readinteger(FILE *f, int b0)
{
    int b1, b2, b3, b4;

    if (b0 == 28)
    {
        b1 = read8(f);
        b2 = read8(f);
        return (short)((b1 << 8) | b2);
    }

    if (b0 == 29)
    {
        b1 = read8(f);
        b2 = read8(f);
        b3 = read8(f);
        b4 = read8(f);
        return (b1 << 24) | (b2 << 16) | (b3 << 8) | b4;
    }

    if (b0 < 247)
    {
        return b0 - 139;
    }

    if (b0 < 251)
    {
        b1 = read8(f);
        return (b0 - 247) * 256 + b1 + 108;
    }

    b1 = read8(f);
    return -(b0 - 251) * 256 - b1 - 108;
}

static char *operatorname(int op)
{
    static char tmpstr[64];

    switch (op)
    {
        /* topdict */
    case 0:	return "version";
    case 1:	return "Notice";
    case 256|0:	return "Copyright";
    case 2:	return "FullName";
    case 3:	return "FamilyName";
    case 4:	return "Weight";
    case 256|1:	return "isFixedPitch";
    case 256|2:	return "ItalicAngle";
    case 256|3:	return "UnderlinePosition";
    case 256|4:	return "UnderlineThickness";
    case 256|5:	return "PaintType";
    case 256|6:	return "CharstringType";
    case 256|7:	return "FontMatrix";
    case 13:	return "UniqueID";
    case 5:	return "FontBBox";
    case 256|8:	return "StrokeWidth";
    case 14:	return "XUID";
    case 15:	return "charset";
    case 16:	return "Encoding";
    case 17:	return "CharStrings";
    case 18:	return "Private";
    case 256|20: return "SyntheticBase";
    case 256|21: return "PostScript";
    case 256|22: return "BaseFontName";
    case 256|23: return "BaseFontBlend";
    case 256|30: return "ROS";
    case 256|31: return "CIDFontVersion";
    case 256|32: return "CIDFontRevision";
    case 256|33: return "CIDFontType";
    case 256|34: return "CIDCount";
    case 256|35: return "UIDBase";
    case 256|36: return "FDArray";
    case 256|37: return "FDSelect";
    case 256|38: return "FontName";

        /* private */
    case 6: return "BlueValues";
    case 7: return "OtherBlues";
    case 8: return "FamilyBlues";
    case 9: return "FamilyOtherBlues";
    case 256|9: return "BlueScale";
    case 256|10: return "BlueShift";
    case 256|11: return "BlueFuzz";
    case 10: return "StdHW";
    case 11: return "StdVW";
    case 256|12: return "StemSnapH";
    case 256|13: return "StemSnapV";
    case 256|14: return "ForceBold";
    case 256|17: return "LanguageGroup";
    case 256|18: return "ExpansionFactor";
    case 256|19: return "initialRandomSeed";
    case 19: return "Subrs";
    case 20: return "defaultWidthX";
    case 21: return "nominalWidthX";
    }

    if (op >= 256)
        sprintf(tmpstr, "12 %d", op & 0xff);
    else
        sprintf(tmpstr, "%d", op);
    return tmpstr;
}

static void readdict(FILE *f, char *dictname, int idx, int sofs, int eofs, int indent)
{
    struct number x[48];
    int n = 0;
    int i;
    int b0;

    if (indent) printf("    ");
    printf("%s %d\n", dictname, idx);

    if (indent) printf("    ");
    printf("{\n", dictname, idx);

    seekpos(f, sofs);
    while (tellpos(f) < eofs)
    {
        b0 = read8(f);
        if (b0 < 22)
        {
            if (b0 == 12)
                b0 = 0x100 | read8(f);

            /* extract some offsets that we need to follow later */

            if (b0 == 15)
                fontinfo[idx].charset = x[0].ival;
            if (b0 == 16)
                fontinfo[idx].encoding = x[0].ival;
            if (b0 == 17)
                fontinfo[idx].charstrings = x[0].ival;
            if (b0 == 18)
            {
                fontinfo[idx].privatelen = x[0].ival;
                fontinfo[idx].privateofs = x[1].ival;
            }
            if (b0 == 19)
                fontinfo[idx].subrs = x[0].ival + sofs;
            if (b0 == (256|36))
                fontinfo[idx].fdarray = x[0].ival;
            if (b0 == (256|37))
                fontinfo[idx].fdselect = x[0].ival;

            /* print the operator name and values */

            if (indent) printf("    ");
            printf("    %-20s = ", operatorname(b0));

            for (i = 0; i < n; i++)
            {
                if (x[i].type == INTEGER)
                    printf("%d ", x[i].ival);
                if (x[i].type == REAL)
                    printf("%0.9g ", x[i].rval);
            }

            printf("\n");

            n = 0;
        }

        else
        {
            if (b0 == 30)
            {
                x[n].type = REAL;
                x[n].rval = readreal(f);
                x[n].ival = x[n].rval; // just in case
                n++;
            }
            else // if (b0 == 28 || b0 == 29 || (b0 >= 32 && b0 <= 254))
            {
                x[n].type = INTEGER;
                x[n].ival = readinteger(f, b0);
                x[n].rval = x[n].ival; // just in case
                n++;
            }
        }
    }

    if (indent) printf("    ");
    printf("}\n\n");
}

void runcode(int idx, unsigned char *code)
{
    int array[32]; // transient array
    int arg[48];
    unsigned char *rets[10];
    unsigned char *pc;
    int n, nret, v, w;
    int nhints;
    int x, y, i;

    pc = code;
    nhints = 0;
    nret = 0;
    n = 0;

    while (1)
    {
        v = *pc++;
        if (v > 31)
        {
            if (v < 247)
            {
                arg[n++] = v - 139;
            }
            else if (v < 251)
            {
                w = *pc++;
                arg[n++] = (v - 247) * 256 + w + 108;
            }
            else if (v < 255)
            {
                w = *pc++;
                arg[n++] = -(v - 251) * 256 - w - 108;
            }
            else
            {
                unsigned a = *pc++ << 24;
                unsigned b = *pc++ << 16;
                unsigned c = *pc++ << 8;
                unsigned d = *pc++;
                arg[n++] = a | b | c | d;
                arg[n-1] = arg[n-1] >> 16; // fixed point (oops)
            }
        }

        else if (v == 28)
        {
            unsigned a = *pc++ << 8;
            unsigned b = *pc++;
            arg[n++] = (short)(a | b);
        }

        else if (v == 12)
        {
            v = *pc++;
            switch (v)
            {
            case 9: // abs
                arg[n-1] = arg[n-1] < 0 ? -arg[n-1] : arg[n-1];
                break;

            case 10: // add
                arg[n-2] = arg[n-2] + arg[n-1];
                n--;
                break;

            case 11: // sub
                arg[n-2] = arg[n-2] - arg[n-1];
                n--;
                break;

            case 12: // div
                arg[n-2] = arg[n-2] / arg[n-1];
                n--;
                break;

            case 14: // neg
                arg[n-1] = -arg[n-1];
                break;

            case 23: // random
                arg[n++] = 0;
                break;

            case 24: // mul
                arg[n-2] = arg[n-2] * arg[n-1];
                n--;
                break;

            case 26: // sqrt
                arg[n-1] = sqrt(arg[n-1]);
                break;

            case 18: // drop
                n--;
                break;

            case 28: // exch
                { int t = arg[n-2]; arg[n-2] = arg[n-1]; arg[n-1] = t; }
                break;

            case 29: // index
                i = arg[--n];
                if (i < 0)
                    i = 0;
                arg[n] = arg[n-1-i];
                n++;
                break;

            case 30: // roll
                {
                    int j = arg[--n];
                    int r = arg[--n];
                    int tmp[n];
                    for (i = 0; i < n; i++)
                        tmp[i] = arg[n-r+i];
                    for (i = 0; i < n; i++)
                        arg[n-r+(i+j)%r] = tmp[i];
                }
                break;

            case 27: // dup
                arg[n] = arg[n-1];
                n++;
                break;


            case 20: // put
                array[arg[n-1]] = arg[n-2];
                n -= 2;
                break;

            case 21: // get
                arg[n-1] = array[arg[n-1]];
                break;


            case 3: // and
                arg[n-2] = arg[n-1] && arg[n-2];
                n--;
                break;

            case 4: // or
                arg[n-2] = arg[n-1] || arg[n-2];
                n--;
                break;

            case 5: // not
                arg[n-1] = !arg[n-1];
                break;

            case 15: // eq
                arg[n-2] = arg[n-2] == arg[n-1];
                break;

            case 22: // ifelse
                {
                    int v2 = arg[--n];
                    int v1 = arg[--n];
                    int s2 = arg[--n];
                    int s1 = arg[--n];
                    arg[n++] = (v1 > v2) ? s2 : s1;
                }
                break;


            case 35: // flex
            case 34: // hflex
            case 36: // hflex1
            case 37: // flex1
                n = 0;
                break;

            }
        }

        else
        {
            switch (v)
            {
            case 1: // hstem
            case 3: // vstem
            case 18: // hstemhm
            case 23: // vstemhm
                i = 0;

                // thes hints take an even number of args,
                // so if we're odd, the width is first.
                if (n & 1)
                {
                    printf("\t%d charwidth\n", arg[0]);
                    i = 1;
                }

                y = 0;
                while (i < n)
                {
                    printf("\t%d %d hintstem\n", arg[i+0] + y, arg[i+1]);
                    y = y + arg[0] + arg[1];
                    nhints ++;
                    i += 2;
                }
                n = 0;
                break;

            case 19: // hintmask
            case 20: // cntrmask
                if (n == 1)
                {
                    printf("\t%d charwidth\n", arg[0]);
                }

                printf("\thintmask (%d hints) ", nhints);
                for (i = 0; i < (nhints + 7) / 8; i++)
                {
                    y = *pc++;
                    printf("0x%02x ", y);
                }
                printf("\n");
                n = 0;
                break;

            case 11: // return
                printf("\treturn\n");
                break;

            case 14: // endchar
                if (n == 1)
                    printf("\t%d charwidth\n", arg[0]);
                printf("\tendchar\n");
                n = 0;
                return;

            case 21: // rmoveto
                if (n == 3)
                {
                    printf("\t%d charwidth\n", arg[0]);
                    printf("\t%d %d rmoveto\n", arg[1], arg[2]);
                }
                else
                    printf("\t%d %d rmoveto\n", arg[0], arg[1]);
                n = 0;
                break;

            case 22: // hmoveto
                if (n == 2)
                {
                    printf("\t%d charwidth\n", arg[0]);
                    printf("\t%d hmoveto\n", arg[1]);
                }
                else
                    printf("\t%d hmoveto\n", arg[0]);
                n = 0;
                break;

            case 4: // vmoveto
                if (n == 2)
                {
                    printf("\t%d charwidth\n", arg[0]);
                    printf("\t%d vmoveto\n", arg[1]);
                }
                else
                    printf("\t%d vmoveto\n", arg[0]);
                n = 0;
                break;

            case 5: // rlineto
                for (i = 0; i < n; i += 2)
                {
                    printf("\t%d %d rlineto\n", arg[i+0], arg[i+1]);
                }
                n = 0;
                break;

            case 6: // hlineto
                for (i = 0; i < n; i++)
                    printf("\t%d %clineto\n", arg[i], i&1 ? 'v' : 'h');
                n = 0;
                break;

            case 7: // vlineto
                for (i = 0; i < n; i++)
                    printf("\t%d %clineto\n", arg[i], i&1 ? 'h' : 'v');
                n = 0;
                break;

            case 8:
            case 27:
            case 31:
            case 24:
            case 25:
            case 30:
            case 26:
                n = 0;

            }
        }
    }
}

void readcode(FILE *f)
{
    int done = 0;
    int stems = 0;
    int v, w;
    int n = 0;
    int x[48];

    while (!done)
    {
        v = getc(f);
        if (v > 31)
        {
            if (v < 247)
            {
                x[n++] = v - 139;
            }
            else if (v < 251)
            {
                w = getc(f);
                x[n++] = (v - 247) * 256 + w + 108;
            }
            else if (v < 255)
            {
                w = getc(f);
                x[n++] = -(v - 251) * 256 - w - 108;
            }
            else
            {
                unsigned a = getc(f) << 24;
                unsigned b = getc(f) << 16;
                unsigned c = getc(f) << 8;
                unsigned d = getc(f);
                unsigned u = a | b | c | d;
                x[n++] = u;
            }
            printf("%d ", x[n-1]);
        }
        else if (v == 28)
        {
            unsigned a = getc(f) << 8;
            unsigned b = getc(f);
            short s = a | b;
            x[n++] = s; // XXX Fixed?
            printf("%d ", x[n-1]);
        }

        else if (v == 19 || v == 20)
        {
            int i;
            if (v == 19) printf("hintmask");
            if (v == 20) printf("cntrmask");
            for (i = 0; i < (stems + 7) / 8; i++)
                printf(" 0x%02x", getc(f));
            printf("\n");
        }

        /* it's an operator */
        else
        {
            switch (v)
            {
            case 0: puts("-Reserved-"); break;
            case 2: puts("-Reserved-"); break;
            case 9: puts("-Reserved-"); break;
            case 13: puts("-Reserved-"); break;
            case 15: puts("-Reserved-"); break;
            case 16: puts("-Reserved-"); break;
            case 17: puts("-Reserved-"); break;
            case 1: puts("hstem"); stems++; break;
            case 3: puts("vstem"); stems++; break;
            case 4: puts("vmoveto"); break;
            case 5: puts("rlineto"); break;
            case 6: puts("hlineto"); break;
            case 7: puts("vlineto"); break;
            case 8: puts("rrcurveto"); break;
            case 10: puts("callsubr"); break;
            case 11: puts("return"); done = 1; break;
            case 14: puts("endchar"); done = 1; break;
            case 18: puts("hstemhm"); stems++; break;
            case 21: puts("rmoveto"); break;
            case 22: puts("hmoveto"); break;
            case 23: puts("vstemhm"); stems++; break;
            case 24: puts("rcurveline"); break;
            case 25: puts("rlinecurve"); break;
            case 26: puts("vvcurveto"); break;
            case 27: puts("hhcurveto"); break;
            case 29: puts("callgsubr"); break;
            case 30: puts("vhcurveto"); break;
            case 31: puts("hvcurveto"); break;
            } 

            if (v == 12)
            {
                w = getc(f);
                switch (w)
                {
                case 0: puts("dotsection"); break; // deprecated noop
                case 1: puts("-Reserved-"); break;
                case 2: puts("-Reserved-"); break;
                case 6: puts("-Reserved-"); break;
                case 7: puts("-Reserved-"); break;
                case 8: puts("-Reserved-"); break;
                case 13: puts("-Reserved-"); break;
                case 16: puts("-Reserved-"); break;
                case 17: puts("-Reserved-"); break;
                case 19: puts("-Reserved-"); break;
                case 3: puts("and"); break;
                case 4: puts("or"); break;
                case 5: puts("not"); break;
                case 9: puts("abs"); break;
                case 10: puts("add"); break;
                case 11: puts("sub"); break;
                case 12: puts("div"); break;
                case 14: puts("neg"); break;
                case 15: puts("eq"); break;
                case 18: puts("drop"); break;
                }
            }

            n = 0;
        }
    }
}

void hexdump(byte *sofs, byte *eofs)
{
    printf("< ");
    while (sofs < eofs)
	printf("%02x ", *sofs++);
    printf("> ");
}

void readheader(FILE *f)
{
    int major = read8(f);
    int minor = read8(f);
    int hdrsize = read8(f);
    int offsize = read8(f);

    printf("version = %d.%d\n", major, minor);

    seekpos(f, hdrsize);
}

void readnameindex(FILE *f)
{
    int i;

    int count = read16(f);
    int offsize = read8(f);
    int offset[count + 1];
    for (i = 0; i < count + 1; i++)
        offset[i] = readoffset(f, offsize);

    int baseofs = tellpos(f) - 1;
    for (i = 0; i < count; i++)
    {
        int n = offset[i+1] - offset[i];
        char buf[n + 1];
        seekpos(f, baseofs + offset[i]);
        fread(buf, 1, n, f);
        buf[n] = 0;
        printf("name %d = (%s)\n", i, buf);
    }

    fontcount = count;

    printf("\n");
}

void readtopdictindex(FILE *f)
{
    int i;
    int count = read16(f);
    int offsize = read8(f);
    int offset[count + 1];
    for (i = 0; i < count + 1; i++)
        offset[i] = readoffset(f, offsize);
    int baseofs = tellpos(f) - 1;
    for (i = 0; i < count; i++)
        readdict(f, "top dict", i, baseofs + offset[i], baseofs + offset[i+1], 0);
}

void readstringindex(FILE *f)
{
    int i;
    int count = read16(f);
    if (count == 0)
        return;
    int offsize = read8(f);
    int offset[count + 1];
    for (i = 0; i < count + 1; i++)
        offset[i] = readoffset(f, offsize);
    int baseofs = tellpos(f) - 1;
    for (i = 0; i < count; i++)
    {
        int n = offset[i+1] - offset[i];
        char buf[n + 1];
        seekpos(f, baseofs + offset[i]);
        fread(buf, 1, n, f);
        buf[n] = 0;
        printf("string %d = (%s)\n", i + 391, buf);
    }
    printf("\n");
}

void readgsubrindex(FILE *f)
{
    int i;
    int count = read16(f);
    if (count == 0)
        return;

    gsubrcount = count;
    gsubrdata = malloc(sizeof(char*) * gsubrcount);

    int offsize = read8(f);
    int offset[count + 1];
    for (i = 0; i < count + 1; i++)
        offset[i] = readoffset(f, offsize);
    int baseofs = tellpos(f) - 1;
    for (i = 0; i < count; i++)
    {
        gsubrdata[i] = malloc(offset[i+1] - offset[i]);
        seekpos(f, baseofs + offset[i]);
        fread(gsubrdata[i], 1, offset[i+1] - offset[i], f);
    }

    printf("loaded %d gsubrs\n\n", gsubrcount);
    for (i = 0; i < count; i++)
    {
	printf("gsubr %d ", i);
	hexdump(gsubrdata[i],
		gsubrdata[i] + offset[i+1] - offset[i]);
	printf("\n");
    }
    printf("\n");
}

/*
 *
 */

void readfdarray(FILE *f, int idx)
{
    int fdarray = fontinfo[idx].fdarray;
    if (fdarray == 0)
        return;
    seekpos(f, fdarray);

    int i;
    int count = read16(f);
    if (count == 0)
        return;
    int offsize = read8(f);
    int offset[count + 1];
    for (i = 0; i < count + 1; i++)
        offset[i] = readoffset(f, offsize);

    int baseofs = tellpos(f) - 1;
    for (i = 0; i < count; i++)
    {
        fontinfo[idx].privatelen = 0;
        fontinfo[idx].privateofs = 0;
        readdict(f, "cid font dict", i, baseofs + offset[i], baseofs + offset[i+1], 1);
        readdict(f, "cid font private dict", i,
                fontinfo[i].privateofs,
                fontinfo[i].privateofs + fontinfo[i].privatelen, 1);
    }
}

void readfdselect(FILE *f, int idx)
{
    int glyphcount = fontinfo[idx].glyphcount;
    int fdselect = fontinfo[idx].fdselect;
    if (fdselect == 0)
        return;
    seekpos(f, fdselect);

    int format = read8(f);
    int i;

    printf("    cid font dict selector format %d\n    {\n", format);

    if (format == 0)
    {
        printf("        ");
        for (i = 0; i < glyphcount; i++)
        {
            int fd = read8(f);
            printf("%-3d ", fd);
            if (i % 15 == 0)
                printf("\n        ");
        }
        printf("\n");
    }

    if (format == 3)
    {
        int nranges = read16(f);
        for (i = 0; i < nranges; i++)
        {
            int first = read16(f);
            int fd = read8(f);
            printf("        range %d -> %d\n", first, fd);
        }
        int sentinel = read16(f);
        printf("        sentinel %d\n", sentinel);
    }

    printf("    }\n\n");
}

void readsubrindex(FILE *f, int idx)
{
    int i;
    int count = read16(f);
    if (count == 0)
        return;
    int offsize = read8(f);
    int offset[count + 1];
    for (i = 0; i < count + 1; i++)
        offset[i] = readoffset(f, offsize);

    fontinfo[idx].subrcount = count;
    fontinfo[idx].subrdata = malloc(sizeof(char*) * count);

    int baseofs = tellpos(f) - 1;
    for (i = 0; i < count; i++)
    {
        seekpos(f, baseofs + offset[i]);
        fontinfo[idx].subrdata[i] = malloc(offset[i+1] - offset[i]);
        fread(fontinfo[idx].subrdata[i], 1, offset[i+1] - offset[i], f);
    }

    printf("    loaded %d local subrs\n", count);
    for (i = 0; i < count; i++)
    {
	printf("    subr %d ", i);
	hexdump(fontinfo[idx].subrdata[i],
		fontinfo[idx].subrdata[i] + offset[i+1] - offset[i]);
	printf("\n");
    }
    printf("\n");
}

void readcharstrings(FILE *f, int idx)
{
    int charstrings = fontinfo[idx].charstrings;
    if (charstrings == 0)
        die("no charstrings for font");
    seekpos(f, charstrings);

    int i;
    int count = read16(f);
    int offsize = read8(f);
    int offset[count + 1];
    for (i = 0; i < count + 1; i++)
        offset[i] = readoffset(f, offsize);

    fontinfo[idx].glyphcount = count;

    printf("    loaded %d charstrings\n", count);

    int baseofs = tellpos(f) - 1;
    for (i = 0; i < count; i++)
    {
        int n = offset[i+1] - offset[i];
        unsigned char code[n];

        printf("    charstring %d ", i);

        // seekpos(f, baseofs + offset[i]);
        // readcode(f);

        seekpos(f, baseofs + offset[i]);
        fread(code, 1, n, f);
        // printf("    {\n");
        // runcode(idx, code);
        // printf("    }\n");

	hexdump(code, code + n);
	printf("\n");
    }

    printf("\n");
}

void readcharset(FILE *f, int idx)
{
    int charset = fontinfo[idx].charset;
    int glyphcount = fontinfo[idx].glyphcount;

    switch (charset)
    {
    case 0: printf("    charset = ISOAdobe\n\n"); return;
    case 1: printf("    charset = Expert\n\n"); return;
    case 2: printf("    charset = ExpertSubset\n\n"); return;
    }

    seekpos(f, charset);

    int format = read8(f);
    int wid = 75;
    int col = 4;
    int i, sid;

    printf("    charset format %d\n    {\n", format);

    if (format == 0)
    {
        printf("        ");
        for (i = 1; i < glyphcount; i++)
        {
            sid = read16(f);
            printf("%-5d ", sid);
            if (i % 15 == 0)
                printf("\n        ");
        }
        printf("\n");
    }

    if (format == 1 || format == 2)
    {
        i = 1;
        while (i < glyphcount)
        {
            int first = read16(f);
            int count;
            if (format == 1)
                count = read8(f) + 1;
            else
                count = read16(f) + 1;
            printf("        range %d + %d\n", first, count);
            i += count;
        }
    }

    printf("    }\n\n");
}

void readencoding(FILE *f, int idx)
{
    int encoding = fontinfo[idx].encoding;

    if (encoding == 0)
    {
        printf("    encoding = Standard\n\n");
        return;
    }

    if (encoding == 1)
    {
        printf("    encoding = Expert\n\n");
        return;
    }

    seekpos(f, encoding);

    int format = read8(f);
    int i;

    printf("    encoding format %d\n    {\n", format);

    if ((format & 0x7f) == 0)
    {
        printf("        ");
        int ncodes = read8(f);
        for (i = 0; i < ncodes; i++)
        {
            int code = read8(f);
            printf("%-3d ", code);
            if (i & 15 == 0)
                printf("\n        ");
        }
        printf("\n");
    }

    if ((format & 0x7f) == 1)
    {
        int nranges = read8(f);
        for (i = 0; i < nranges; i++)
        {
            int first = read8(f);
            int count = read8(f) + 1;
            printf("        range %d + %d\n", first, count);
        }
    }

    if (format & 0x80)
    {
        int nsups = read8(f);
        for (i = 0; i < nsups; i++)
        {
            int code = read8(f);
            int sid = read16(f);
            printf("        supplement %d = %d\n", code, sid);
        }
    }

    printf("    }\n\n");
}

/*
 *
 */

void readfontset(FILE *f)
{
    int i;
    memset(fontinfo, 0, sizeof fontinfo);

    /* read the fixed-in-place tables */
    readheader(f);
    readnameindex(f);
    readtopdictindex(f);
    readstringindex(f);
    readgsubrindex(f);

    /* load the private dict (we need the subrs offset) */
    for (i = 0; i < fontcount; i++)
    {
        printf("font %d\n{\n", i);

        readdict(f, "private dict", 0,
                fontinfo[i].privateofs,
                fontinfo[i].privateofs + fontinfo[i].privatelen, 1);

        if (fontinfo[i].subrs)
        {
            seekpos(f, fontinfo[i].subrs);
            readsubrindex(f, i);
        }

        /* load the char strings (we need the glyphcount) */
        readcharstrings(f, i);

        readcharset(f, i);
        readencoding(f, i);

        /* this will mess with the private and subr fields */
        readfdarray(f, i);

        readfdselect(f, i);

        printf("}\n\n");
    }
}

/*
 *
 */

int main(int argc, char **argv)
{
    char magic[4];
    FILE *fp;
    int i, n;

    if (argc != 2)
        die("usage: cffdump font.cff");

    fp = fopen(argv[1], "rb");
    if (!fp)
        die("cannot open file '%s': %s", argv[1], strerror(errno));

    fread(magic, 1, 4, fp);
    if (!memcmp(magic, "OTTO", 4))
    {
        cffpos = 0;

        printf("opentype\n{\n");

        int numtables = read16(fp);
        read16(fp); // search range
        read16(fp); // entry selector
        read16(fp); // range shift

        for (i = 0; i < numtables; i++)
        {
            char name[5];
            fread(name, 1, 4, fp);
            name[5] = 0;
            int checksum = read32(fp);
            int offset = read32(fp);
            int length = read32(fp);
            printf("    table (%s) at %8d + %d\n", name, offset, length);
            if (!memcmp(name, "CFF ", 4))
            {
                cffpos = offset;
                cfflen = length;
            }
        }

        if (cffpos == 0)
            die("no CFF table found!");

        printf("}\n\n");
    }

    else
    {
        fseek(fp, 0, 2);
        cffpos = 0;
        cfflen = ftell(fp);
    }

    fseek(fp, cffpos, 0);
    readfontset(fp);

    fclose(fp);
}

