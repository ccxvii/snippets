int * ucs_from_utf8(int *buffer, char *str, int n)
{
    stb_uint32 c;
    int i=0;
    --n;
    while (*str) {
        if (i >= n)
            return NULL;
        if (!(*str & 0x80))
            buffer[i++] = *str++;
        else if ((*str & 0xe0) == 0xc0) {
            c = (*str++ & 0x1f) << 6;
            if ((*str & 0xc0) != 0x80) return NULL;
            buffer[i++] = c + (*str++ & 0x3f);
        } else if ((*str & 0xf0) == 0xe0) {
            c = (*str++ & 0x0f) << 12;
            if ((*str & 0xc0) != 0x80) return NULL;
            c += (*str++ & 0x3f) << 6;
            if ((*str & 0xc0) != 0x80) return NULL;
            buffer[i++] = c + (*str++ & 0x3f);
        } else if ((*str & 0xf8) == 0xf0) {
            c = (*str++ & 0x07) << 18;
            if ((*str & 0xc0) != 0x80) return NULL;
            c += (*str++ & 0x3f) << 12;
            if ((*str & 0xc0) != 0x80) return NULL;
            c += (*str++ & 0x3f) << 6;
            if ((*str & 0xc0) != 0x80) return NULL;
            c += (*str++ & 0x3f);
            // surrogate pair values are invalid
            if ((c & 0xFFFFF800) == 0xD800) return NULL;
            if (c >= 0x10000) {
                c -= 0x10000;
                if (i + 2 > n) return NULL;
                buffer[i++] = 0xD800 | (0x3ff & (c >> 10));
                buffer[i++] = 0xDC00 | (0x3ff & (c      ));
            }
        } else
            return NULL;
    }
    buffer[i] = 0;
    return buffer;
}

char * utf8_from_ucs(char *buffer, int *str, int n)
{
    int i=0;
    --n;
    while (*str) {
        if (*str < 0x80) {
            if (i+1 > n) return NULL;
            buffer[i++] = (char) *str++;
        } else if (*str < 0x800) {
            if (i+2 > n) return NULL;
            buffer[i++] = 0xc0 + (*str >> 6);
            buffer[i++] = 0x80 + (*str & 0x3f);
            str += 1;
        } else if (*str >= 0xd800 && *str < 0xdc00) {
            stb_uint32 c;
            if (i+4 > n) return NULL;
            c = ((str[0] - 0xd800) << 10) + ((str[1]) - 0xdc00) + 0x10000;
            buffer[i++] = 0xf0 + (c >> 18);
            buffer[i++] = 0x80 + ((c >> 12) & 0x3f);
            buffer[i++] = 0x80 + ((c >>  6) & 0x3f);
            buffer[i++] = 0x80 + ((c      ) & 0x3f);
            str += 2;
        } else if (*str >= 0xdc00 && *str < 0xe000) {
            return NULL;
        } else {
            if (i+3 > n) return NULL;
            buffer[i++] = 0xe0 + (*str >> 12);
            buffer[i++] = 0x80 + ((*str >> 6) & 0x3f);
            buffer[i++] = 0x80 + ((*str     ) & 0x3f);
            str += 1;
        }
    }
    buffer[i] = 0;
    return buffer;
}
