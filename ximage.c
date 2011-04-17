#include <stdlib.h>
#include <xcb/xcb.h>

#ifndef restrict
#ifndef _C99
#ifdef __GNUC__
#define restrict __restrict__
#else
#define restrict
#endif
#endif
#endif

typedef unsigned char byte;

static int native_byte_order(void)
{
	static const int one = 1;
	return *(char*)&one == 0;
}

static int roundup(int base, int pad)
{
	int b = base + pad - 1;
	/* faster if pad is a power of two */
	if (((pad - 1) & pad) == 0)
		return b & -pad;
	return b - b % pad;
}

static int ffs(uint32_t x)
{
	int r = 1;
	if (!x) return 0;
	if (!(x & 0xffff)) { x >>= 16; r += 16; }
	if (!(x & 0xff)) { x >>= 8; r += 8; }
	if (!(x & 0xf)) { x >>= 4; r += 4; }
	if (!(x & 3)) { x >>= 2; r += 2; }
	if (!(x & 1)) { x >>= 1; r += 1; }
	return r;
}

enum {
	RGBA8888, BGRA8888,
	ARGB8888, ABGR8888,
	RGB888, BGR888,
	RGB565, RGB565_BR,
	RGB555, RGB555_BR,
	BGR233, UNKNOWN
};

struct ximage {
	int depth, bpp, pad, mode;
	int width, height, stride, size;
	byte *data;
};

static xcb_format_t *
find_format_by_depth (const xcb_setup_t *setup, int depth)
{
	xcb_format_t *fmt = xcb_setup_pixmap_formats(setup);
	xcb_format_t *fmtend = fmt + xcb_setup_pixmap_formats_length(setup);
	for(; fmt != fmtend; ++fmt)
		if(fmt->depth == depth)
			return fmt;
	return 0;
}

static int
find_depth_by_id(xcb_screen_t *screen, int id)
{
	xcb_depth_iterator_t i;
	xcb_visualtype_iterator_t j;
	for (i = xcb_screen_allowed_depths_iterator(screen); i.rem; xcb_depth_next(&i))
		for (j = xcb_depth_visuals_iterator(i.data); j.rem; xcb_visualtype_next(&j))
			if (j.data->visual_id == id)
				return i.data->depth;
	return 0;
}

static xcb_visualtype_t *
find_visual_by_id (xcb_screen_t *screen, int id)
{
	xcb_depth_iterator_t i;
	xcb_visualtype_iterator_t j;
	for (i = xcb_screen_allowed_depths_iterator(screen); i.rem; xcb_depth_next(&i))
		for (j = xcb_depth_visuals_iterator(i.data); j.rem; xcb_visualtype_next(&j))
			if (j.data->visual_id == id)
				return j.data;
	return 0;
}

struct ximage *
ximage_create(xcb_connection_t *conn, xcb_screen_t *screen, xcb_visualid_t id)
{
	struct ximage *img;

	const xcb_setup_t *setup = xcb_get_setup(conn);
	int order = setup->image_byte_order;
	int swap = order != native_byte_order();
	int depth = find_depth_by_id(screen, id);
	xcb_visualtype_t *visual = find_visual_by_id(screen, id);
	xcb_format_t *format = find_format_by_depth(setup, depth);
	int bpp = format->bits_per_pixel;
	int pad = format->scanline_pad;

	uint32_t rm = visual->red_mask;
	uint32_t gm = visual->green_mask;
	uint32_t bm = visual->blue_mask;

	int rs = ffs(rm) - 1;
	int gs = ffs(gm) - 1;
	int bs = ffs(bm) - 1;

	int mode = UNKNOWN;
	if (bpp == 8) {
		mode = BGR233; /* Must be TrueColor! */
	} else if (bpp == 16) {
		if (rm == 0xF800 && gm == 0x07E0 && bm == 0x001F)
			mode = swap ? RGB565_BR : RGB565;
		if (rm == 0x7C00 && gm == 0x03E0 && bm == 0x001F)
			mode = swap ? RGB555_BR : RGB555;
	} else if (bpp == 24) {
		if (rs == 0 && gs == 8 && bs == 16)
			mode = order ? RGB888 : BGR888;
		if (rs == 16 && gs == 8 && bs == 0)
			mode = order ? BGR888 : RGB888;
	} else if (bpp == 32) {
		if (rs == 0 && gs == 8 && bs == 16)
			mode = order ? ABGR8888 : RGBA8888;
		if (rs == 8 && gs == 16 && bs == 24)
			mode = order ? BGRA8888 : ARGB8888;
		if (rs == 16 && gs == 8 && bs == 0)
			mode = order ? ARGB8888 : BGRA8888;
		if (rs == 24 && gs == 16 && bs == 8)
			mode = order ? RGBA8888 : ABGR8888;
	}
	if (mode == UNKNOWN)
		return NULL;

	img = malloc(sizeof(struct ximage));

	img->depth = depth;
	img->bpp = bpp;
	img->pad = pad;
	img->mode = mode;

	img->width = 0;
	img->height = 0;
	img->stride = 0;
	img->size = 0;
	img->data = NULL;

	return img;
}

void
ximage_destroy(struct ximage *img)
{
	free(img->data);
	free(img);
}

/* Convert from BGRA to various formats */

#define B(x) x[0]
#define G(x) x[1]
#define R(x) x[2]
#define A(x) x[3]
#define SS 4

static void
convert_argb8888(byte * restrict dst, const byte * restrict src, int n)
{
	while (n--) {
		dst[0] = A(src);
		dst[1] = R(src);
		dst[2] = G(src);
		dst[3] = B(src);
		dst += 4;
		src += 4;
	}
}

static void
convert_bgra8888(byte * restrict dst, const byte * restrict src, int n)
{
	while (n--) {
		dst[0] = B(src);
		dst[1] = G(src);
		dst[2] = R(src);
		dst[3] = A(src);
		dst += 4;
		src += 4;
	}
}

static void
convert_abgr8888(byte * restrict dst, const byte * restrict src, int n)
{
	while (n--) {
		dst[0] = A(src);
		dst[1] = B(src);
		dst[2] = G(src);
		dst[3] = R(src);
		dst += 4;
		src += 4;
	}
}

static void
convert_rgba8888(byte * restrict dst, const byte * restrict src, int n)
{
	while (n--) {
		dst[0] = R(src);
		dst[1] = G(src);
		dst[2] = B(src);
		dst[3] = A(src);
		dst += 4;
		src += 4;
	}
}

static void
convert_bgr888(byte * restrict dst, const byte * restrict src, int n)
{
	while (n--) {
		dst[0] = B(src);
		dst[1] = G(src);
		dst[2] = R(src);
		dst += 3;
		src += 4;
	}
}

static void
convert_rgb888(byte * restrict dst, const byte * restrict src, int n)
{
	while (n--) {
		dst[0] = R(src);
		dst[1] = G(src);
		dst[2] = B(src);
		dst += 3;
		src += 4;
	}
}

static void
convert_rgb565(byte * restrict dst, const byte * restrict src, int n)
{
	unsigned short *d = (unsigned short *)dst;
	while (n--) {
		byte r = R(src);
		byte g = G(src);
		byte b = B(src);
		*d++ = (r & 0xf8) << 8 | (g & 0xfc) << 3 | b >> 3;
		src += 4;
	}
}

static void
convert_rgb565_br(byte * restrict dst, const byte * restrict src, int n)
{
	unsigned short *d = (unsigned short *)dst;
	while (n--) {
		byte r = R(src);
		byte g = G(src);
		byte b = B(src);
		/* g4 g3 g2 b7 b6 b5 b4 b3 : r7 r6 r5 r4 r3 g7 g6 g5 */
		*d++ = (r & 0xF8) |
			(g & 0xE0) >> 5 |
			(g & 0x1C) << 11 |
			(b & 0xF8) << 5;
		src += 4;
	}
}

static void
convert_rgb555(byte * restrict dst, const byte * restrict src, int n)
{
	unsigned short *d = (unsigned short *)dst;
	while (n--) {
		byte r = R(src);
		byte g = G(src);
		byte b = B(src);
		*d++ = (r & 0xF8) << 7 | (g & 0xF8) << 2 | b >> 3;
		src += 4;
	}
}

static void
convert_rgb555_br(byte * restrict dst, const byte * restrict src, int n)
{
	unsigned short *d = (unsigned short *)dst;
	while (n--) {
		byte r = R(src);
		byte g = G(src);
		byte b = B(src);
		/* g5 g4 g3 b7 b6 b5 b4 b3 : 0 r7 r6 r5 r4 r3 g7 g6 */
		*d++ = (r & 0xF8) >> 1 |
			(g & 0xC0) >> 6 |
			(g & 0x38) << 10 |
			(b & 0xF8) << 5;
		src += 4;
	}
}

static void
convert_bgr233(byte * restrict dst, const byte * restrict src, int n)
{
	while (n--) {
		byte r = R(src);
		byte g = G(src);
		byte b = B(src);
		/* b7 b6 g7 g6 g5 r7 r6 r5 */
		*dst++ = (b & 0xC0) | ((g >> 2) & 0x38) | ((r >> 5) & 0x7);
		src += 4;
	}
}

static void
ximage_convert(struct ximage *dst, unsigned char *sp)
{
	byte *dp = dst->data;
	int y;
	for (y = 0; y < dst->height; y++) {
		switch (dst->mode) {
		case RGBA8888: convert_rgba8888(dp, sp, dst->width); break;
		case BGRA8888: convert_bgra8888(dp, sp, dst->width); break;
		case ARGB8888: convert_argb8888(dp, sp, dst->width); break;
		case ABGR8888: convert_abgr8888(dp, sp, dst->width); break;
		case RGB888: convert_rgb888(dp, sp, dst->width); break;
		case BGR888: convert_bgr888(dp, sp, dst->width); break;
		case RGB565: convert_rgb565(dp, sp, dst->width); break;
		case RGB565_BR: convert_rgb565_br(dp, sp, dst->width); break;
		case RGB555: convert_rgb555(dp, sp, dst->width); break;
		case RGB555_BR: convert_rgb555_br(dp, sp, dst->width); break;
		case BGR233: convert_bgr233(dp, sp, dst->width); break;
		}
		dp += dst->stride;
		sp += dst->width << 2;
	}
}

static void
ximage_resize(xcb_connection_t *conn, struct ximage *img, int width, int height)
{
	if (img->width == width && img->height == height)
		return;

	img->width = width;
	img->height = height;
	img->stride = roundup(img->width * img->bpp, img->pad) >> 3;
	img->size = img->height * img->stride;

	free(img->data);
	img->data = malloc(img->size);
}

xcb_void_cookie_t
ximage_draw(xcb_connection_t *conn, xcb_drawable_t draw, xcb_gcontext_t gc,
	struct ximage *img, int x, int y, int w, int h, unsigned char *bgra)
{
	if (img->mode == BGRA8888) {
		return xcb_put_image(conn, XCB_IMAGE_FORMAT_Z_PIXMAP, draw, gc,
			w, h, x, y, 0, img->depth,
			w * h * 4, bgra);
	} else {
		ximage_resize(conn, img, w, h);
		ximage_convert(img, bgra);
		return xcb_put_image(conn, XCB_IMAGE_FORMAT_Z_PIXMAP, draw, gc,
			img->width, img->height, x, y, 0, img->depth,
			img->size, img->data);
	}
}
