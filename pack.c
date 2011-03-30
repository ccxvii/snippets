/**
 * pack.c  -- perl/python-esque pack/unpack functions
 *
 * like printf and scanf, but for binary data
 *
 * Kopyleft (K) 2000 Mazirian
 *
 * TODO: doubles (dD), smarter strings... (aXXX)
 *
 */

/*
 * csif    - uchar/short/int/float - little-endian (intel)
 * CSIF    - big-endian (motorola/network)
 * 0-9     - string of length (0-9)
 */

#include <stdio.h>
#include <stdarg.h>

#include "pack.h"

/* ------------------------------------------------------------------------ */

int spack(unsigned char *buf, unsigned char *format, ...)
{
    va_list ap;
    unsigned char *bp = buf;
    unsigned char *fp = format;
    unsigned char *str;
    unsigned char c;
    short s;
    int i;

    va_start(ap, format);

    do {
        switch (*fp) {
            case 'c':
            case 'C':
                c = va_arg(ap, unsigned char);
                *bp++ = c;
                break;

            case 's':
                s = va_arg(ap, short);
                *bp++ = (s >> 0) & 0xFF;
                *bp++ = (s >> 8) & 0xFF;
                break;

            case 'S':
                s = va_arg(ap, short);
                *bp++ = (s >> 8) & 0xFF;
                *bp++ = (s >> 0) & 0xFF;
                break;

            case 'i':
            case 'f':
                i = va_arg(ap, int);
                *bp++ = (s >> 0) & 0xFF;
                *bp++ = (s >> 8) & 0xFF;
                *bp++ = (s >> 16) & 0xFF;
                *bp++ = (s >> 24) & 0xFF;
                break;

            case 'I':
            case 'F':
                i = va_arg(ap, int);
                *bp++ = (s >> 24) & 0xFF;
                *bp++ = (s >> 16) & 0xFF;
                *bp++ = (s >> 8) & 0xFF;
                *bp++ = (s >> 0) & 0xFF;
                break;

            default:
                if (*fp >= '0' && *fp <= '9') {
                    str = va_arg(ap, unsigned char*);
                    for (i=0; i < *fp - '0'; i++) {
                        *bp++ = *str++;
                    }
                } else {
                    printf("fpack: bad format (%d)\n", *fp);
                }
                break;
        }
    } while (*(++fp));

    va_end(ap);
    return (bp-buf);
}

/* ------------------------------------------------------------------------ */

int fpack(FILE *file, unsigned char *format, ...)
{
    va_list ap;
    unsigned char *fp = format;
    unsigned char *str;
    unsigned char c;
    short s;
    int i;

    va_start(ap, format);

    do {
        switch (*fp) {
            case 'c':
            case 'C':
                c = va_arg(ap, unsigned char);
                putc(c,file);
                break;

            case 's':
                s = va_arg(ap, short);
                putc((s >> 0) & 0xFF, file);
                putc((s >> 8) & 0xFF, file);
                break;

            case 'S':
                s = va_arg(ap, short);
                putc((s >> 8) & 0xFF, file);
                putc((s >> 0) & 0xFF, file);
                break;

            case 'i':
            case 'f':
                i = va_arg(ap, int);
                putc((s >> 0) & 0xFF, file);
                putc((s >> 8) & 0xFF, file);
                putc((s >> 16) & 0xFF, file);
                putc((s >> 24) & 0xFF, file);
                break;

            case 'I':
            case 'F':
                i = va_arg(ap, int);
                putc((s >> 24) & 0xFF, file);
                putc((s >> 16) & 0xFF, file);
                putc((s >> 8) & 0xFF, file);
                putc((s >> 0) & 0xFF, file);
                break;

            default:
                if (*fp >= '0' && *fp <= '9') {
                    str = va_arg(ap, unsigned char*);
                    for (i=0; i < *fp - '0'; i++) {
                        putc(*str++, file);
                    }
                } else {
                    printf("fpack: bad format (%d)\n", *fp);
                }
                break;
        }
    } while (*(++fp));

    va_end(ap);
    return 1;
}

/* ------------------------------------------------------------------------ */

int sunpack(unsigned char *buf, unsigned char *format, ...)
{
    va_list ap;
    unsigned char *bp = buf;
    unsigned char *fp = format;
    unsigned char *str;
    unsigned char c;
    short s;
    int i;

    va_start(ap, format);

    do {
        switch (*fp) {
            case 'c':
            case 'C':
                c = *bp++;
                *(va_arg(ap, unsigned char*)) = c;
                break;

            case 's':
                s = (*bp++) << 0;
                s |= (*bp++) << 8;
                *(va_arg(ap, short*)) = s;
                break;

            case 'S':
                s = (*bp++) << 8;
                s |= (*bp++) << 0;
                *(va_arg(ap, short*)) = s;
                break;

            case 'i':
            case 'f':
                i = (*bp++) << 0;
                i |= (*bp++) << 8;
                i |= (*bp++) << 16;
                i |= (*bp++) << 24;
                *(va_arg(ap, int*)) = i;
                break;

            case 'I':
            case 'F':
                i = (*bp++) << 24;
                i |= (*bp++) << 16;
                i |= (*bp++) << 8;
                i |= (*bp++) << 0;
                *(va_arg(ap, int*)) = i;
                break;

            default:
                if (*fp >= '0' && *fp <= '9') {
                    str = va_arg(ap, unsigned char*);
                    for (i=0; i < *fp - '0'; i++) {
                        *str++ = *bp++;
                    }
                } else {
                    printf("sunpack: bad format (%d)\n", *fp);
                }
                break;
        }
    } while (*(++fp));

    va_end(ap);
    return bp-buf;
}

/* ------------------------------------------------------------------------ */

int funpack(FILE *file, unsigned char *format, ...)
{
    va_list ap;
    unsigned char *fp = format;
    unsigned char *str;
    unsigned char c;
    short s;
    int i;

    va_start(ap, format);

    do {
        switch (*fp) {
            case 'c':
            case 'C':
                c = getc(file);
                *(va_arg(ap, unsigned char*)) = c;
                break;

            case 's':
                s = getc(file) << 0;
                s |= getc(file) << 8;
                *(va_arg(ap, short*)) = s;
                break;

            case 'S':
                s = getc(file) << 8;
                s |= getc(file) << 0;
                *(va_arg(ap, short*)) = s;
                break;

            case 'i':
            case 'f':
                i = getc(file) << 0;
                i |= getc(file) << 8;
                i |= getc(file) << 16;
                i |= getc(file) << 24;
                *(va_arg(ap, int*)) = i;
                break;

            case 'I':
            case 'F':
                i = getc(file) << 24;
                i |= getc(file) << 16;
                i |= getc(file) << 8;
                i |= getc(file) << 0;
                *(va_arg(ap, int*)) = i;
                break;

            default:
                if (*fp >= '0' && *fp <= '9') {
                    str = va_arg(ap, unsigned char*);
                    for (i=0; i < *fp - '0'; i++) {
                        *str++ = getc(file);
                    }
                } else {
                    printf("funpack: bad format (%d)\n", *fp);
                }
                break;
        }
    } while (*(++fp));

    va_end(ap);
    return 1;
}

/* ------------------------------------------------------------------------ */
#if 1
int main (int argc, char **argv) {
    FILE *fp = fopen("Bunny.ply", "rb");
    unsigned char ply[4] = {'A', 'S', 'D', 0};
    int version, nattr; unsigned char r, g, b; float diff;
    funpack(fp, "3iicccf", ply, &version, &nattr, &r, &g, &b, &diff);
    printf("%s - %d / %d: %d %d %d - %f\n",ply,version,nattr,r,g,b,diff);
}
#endif
