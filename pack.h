/**
 * pack.h  -- perl/python-esque pack/unpack functions
 * 
 * like printf and scanf, but for binary data
 *
 * Kopyleft (K) 2000 Mazirian
 *
 */

/*
 * csif    - uchar/short/int/float - little-endian (intel)
 * CSIF    - big-endian (motorola/network)
 * 0-9     - string of length (0-9)
 */

/* pack.c */
#ifndef _PACK_H_
#define _PACK_H_

#include <stdio.h>
#include <stdarg.h>

int spack(unsigned char *buf, unsigned char *format, ...);
int fpack(FILE *file, unsigned char *format, ...);
int sunpack(unsigned char *buf, unsigned char *format, ...);
int funpack(FILE *file, unsigned char *format, ...);

#endif

