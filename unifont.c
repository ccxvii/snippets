#include <stdio.h>
#include <stdlib.h>

#include <zlib.h>

struct unifont
{
	int count;
	unsigned short *low;
	unsigned short *high;
	unsigned char *width;
	int *offset;
	unsigned char *data;
};

static inline int readlong(FILE *f)
{
	int a = getc(f);
	int b = getc(f);
	int c = getc(f);
	int d = getc(f);
	return a << 24 | b << 16 | c << 8 | d;
}

struct unifont *
ui_load_unifont(char *filename)
{
	struct unifont *u;
	unsigned char *cdata;
	int count, csize;
	uLongf usize;
	int i;
	FILE *f;

	f = fopen(filename, "rb");
	if (!f)
		return NULL;

	u = malloc(sizeof(struct unifont));
	count = readlong(f);
	csize = readlong(f);
	usize = readlong(f);

	u->count = count;
	u->low = malloc(count * sizeof(unsigned short));
	u->high = malloc(count * sizeof(unsigned short));
	u->width = malloc(count * sizeof(unsigned char));
	u->offset = malloc(count * sizeof(int));
	u->data = malloc(usize);

	for (i = 0; i < u->count; i++)
	{
		u->low[i] = readlong(f);
		u->high[i] = readlong(f);
		u->width[i] = readlong(f);
		u->offset[i] = readlong(f);
	}

	cdata = malloc(csize);
	fread(cdata, 1, csize, f);
	uncompress(u->data, &usize, cdata, csize);
	free(cdata);

	fclose(f);

	return u;
}

static int
ui_find_unifont_range(struct unifont *u, int ucs)
{
	int l = 0;
	int r = u->count - 1;
	int m;
	while (l <= r)
	{
		m = (l + r) >> 1;
		if (ucs < u->low[m])
			r = m - 1;
		else if (ucs > u->high[m])
			l = m + 1;
		else
			return m;
	}
	return -1;
}

int
ui_measure_unifont_char(struct unifont *u, int ucs)
{
	int m = ui_find_unifont_range(u, ucs);
	if (m > -1)
		return u->width[m];
	return 0;
}

static void
ui_draw_unifont_glyph(unsigned char *p, int w)
{
	int x, k, y;
	for (y = 0; y < 16; y++)
	{
		for (k = 0; k < w; k++)
		{
			for (x = 0; x < 8; x++)
			{
				if ((*p >> (7 - x)) & 1) {
					putchar('#');
					putchar('#');
				} else {
					putchar('.');
					putchar('.');
				}
			}
			p++;
		}
		putchar('\n');
	}
}

int
ui_draw_unifont_char(struct unifont *u, int ucs)
{
	int m = ui_find_unifont_range(u, ucs);
	if (m > -1)
	{
		int i = ucs - u->low[m];
		int w = u->width[m];
		unsigned char *p = u->data + u->offset[m] + i * w * 16;
		ui_draw_unifont_glyph(p, w, x, y, dst);
		return w;
	}
	return 0;
}

int
main(int argc, char **argv)
{
	struct unifont *u;
	int c, i;

	u = ui_load_unifont("unifont.dat");
	for (i = 1; i < argc; i++)
	{
		c = strtol(argv[i], NULL, 16);
		ui_draw_unifont_char(u, c, 0, 0, NULL);
		putchar('\n');
	}

	return 0;
}

