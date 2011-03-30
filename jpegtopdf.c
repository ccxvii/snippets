/*
 * Convert series of JPEG files into a comic-book format PDF.
 * Center images on an A4 page, rotate landscape spreads.
 */

/* A4: 595 x 842 */
enum
{
        PAGEW = 595,
        PAGEH = 842,
        MARGIN = 36
};

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

unsigned int pagew = PAGEW;
unsigned int pageh = PAGEH;
unsigned int margin = MARGIN;
int center = 1;
unsigned int dpi = 72;

int pages[8000];
int npages = 0;

int xref[8000*3+100];
int nxref = 0;

typedef struct {
  FILE     *fp;                   /* file pointer for jpeg file          */
  char     *filename;             /* name of image file                  */
  int      filesize;
  int      width;                 /* pixels per line                     */
  int      height;                /* rows                                */
  int      components;            /* number of color components          */
  int      bits_per_component;    /* bits per color component            */
  float    dpi;                   /* image resolution in dots per inch   */
} imagedata;

enum { FALSE, TRUE };

/* JPEG marker codes stolen from the IJG JPEG library.  */
enum
{
        M_SOF0  = 0xc0, /* baseline DCT                         */
        M_SOF1  = 0xc1, /* extended sequential DCT              */
        M_SOF2  = 0xc2, /* progressive DCT                      */
        M_SOF3  = 0xc3, /* lossless (sequential)                */

        M_SOF5  = 0xc5, /* differential sequential DCT          */
        M_SOF6  = 0xc6, /* differential progressive DCT         */
        M_SOF7  = 0xc7, /* differential lossless                */

        M_JPG   = 0xc8, /* JPEG extensions                      */
        M_SOF9  = 0xc9, /* extended sequential DCT              */
        M_SOF10 = 0xca, /* progressive DCT                      */
        M_SOF11 = 0xcb, /* lossless (sequential)                */

        M_SOF13 = 0xcd, /* differential sequential DCT          */
        M_SOF14 = 0xce, /* differential progressive DCT         */
        M_SOF15 = 0xcf, /* differential lossless                */

        M_DHT   = 0xc4, /* define Huffman tables                */

        M_DAC   = 0xcc, /* define arithmetic conditioning table */

        M_RST0  = 0xd0, /* restart                              */
        M_RST1  = 0xd1, /* restart                              */
        M_RST2  = 0xd2, /* restart                              */
        M_RST3  = 0xd3, /* restart                              */
        M_RST4  = 0xd4, /* restart                              */
        M_RST5  = 0xd5, /* restart                              */
        M_RST6  = 0xd6, /* restart                              */
        M_RST7  = 0xd7, /* restart                              */

        M_SOI   = 0xd8, /* start of image                       */
        M_EOI   = 0xd9, /* end of image                         */
        M_SOS   = 0xda, /* start of scan                        */
        M_DQT   = 0xdb, /* define quantization tables           */
        M_DNL   = 0xdc, /* define number of lines               */
        M_DRI   = 0xdd, /* define restart interval              */
        M_DHP   = 0xde, /* define hierarchical progression      */
        M_EXP   = 0xdf, /* expand reference image(s)            */

        M_APP0  = 0xe0, /* application marker, used for JFIF    */
        M_APP1  = 0xe1, /* application marker                   */
        M_APP2  = 0xe2, /* application marker                   */
        M_APP3  = 0xe3, /* application marker                   */
        M_APP4  = 0xe4, /* application marker                   */
        M_APP5  = 0xe5, /* application marker                   */
        M_APP6  = 0xe6, /* application marker                   */
        M_APP7  = 0xe7, /* application marker                   */
        M_APP8  = 0xe8, /* application marker                   */
        M_APP9  = 0xe9, /* application marker                   */
        M_APP10 = 0xea, /* application marker                   */
        M_APP11 = 0xeb, /* application marker                   */
        M_APP12 = 0xec, /* application marker                   */
        M_APP13 = 0xed, /* application marker                   */
        M_APP14 = 0xee, /* application marker, used by Adobe    */
        M_APP15 = 0xef, /* application marker                   */

        M_JPG0  = 0xf0, /* reserved for JPEG extensions         */
        M_JPG13 = 0xfd, /* reserved for JPEG extensions         */
        M_COM   = 0xfe, /* comment                              */

        M_TEM   = 0x01, /* temporary use                        */

        M_ERROR = 0x100 /* dummy marker, internal use only      */
};

/* read two byte parameter, MSB first */
static inline unsigned int getshort(FILE *fp)
{
        unsigned int val;
        val = (unsigned int) (getc(fp) << 8);
        val += (unsigned int) (getc(fp));
        return val;
}

/* look for next JPEG Marker  */
static inline int nextmarker(FILE *fp)
{
        int c;

        if (feof(fp))
                return M_ERROR; /* dummy marker               */

        do {
                /* skip to FF */
                do { c = getc(fp); } while (c != 0xFF);

                /* skip repeated FFs */
                do { c = getc(fp); } while (c == 0xFF);

                /* repeat if FF/00 */
        } while (c == 0x00);

        return c;
}

int scanjpeg(imagedata *image)
{
        int b, c, unit;
        unsigned long i, length = 0;
        #define APP_MAX 255
        unsigned char appstring[APP_MAX];
        int SOF_done = FALSE;

        /* process JPEG markers */
        while (!SOF_done && (c = nextmarker(image->fp)) != M_EOI)
        {
                switch (c)
                {
                case M_ERROR:
                        fprintf(stderr, "Error: unexpected end of JPEG file!\n");
                        return FALSE;

                /* The following are not officially supported in PostScript level 2 */
                case M_SOF2:
                case M_SOF3:
                case M_SOF5:
                case M_SOF6:
                case M_SOF7:
                case M_SOF9:
                case M_SOF10:
                case M_SOF11:
                case M_SOF13:
                case M_SOF14:
                case M_SOF15:
                        fprintf(stderr, "Warning: JPEG file uses compression method %X - proceeding anyway.\n", c);
                        fprintf(stderr, "PostScript output does not work on all PS interpreters!\n");
                        /* FALLTHROUGH */

                case M_SOF0:
                case M_SOF1:
                        length = getshort(image->fp);    /* read segment length  */

                        image->bits_per_component = getc(image->fp);
                        image->height = getshort(image->fp);
                        image->width = getshort(image->fp);
                        image->components = getc(image->fp);

                        SOF_done = TRUE;
                        break;

                /* check for JFIF marker with resolution */
                case M_APP0:
                        length = getshort(image->fp);

                        for (i = 0; i < length - 2; i++) {
                                b = getc(image->fp);
                                if (i < sizeof appstring)
                                        appstring[i] = (unsigned char) b;
                        }

                        /* Check for JFIF application marker and read density values
                         * per JFIF spec version 1.02.
                         * We only check X resolution, assuming X and Y resolution are equal.
                         * Use values only if resolution not preset by user or to be ignored.
                         */

                        #define ASPECT_RATIO    0       /* JFIF unit byte: aspect ratio only */
                        #define DOTS_PER_INCH   1       /* JFIF unit byte: dots per inch     */
                        #define DOTS_PER_CM     2       /* JFIF unit byte: dots per cm       */

                        if (length >= 14 && !strncmp((const char *)appstring, "JFIF", 4))
                        {
                                unit = appstring[7];    /* resolution unit */
                                image->dpi = (float) ((appstring[8]<<8) + appstring[9]);
                                /* resolution value */

                                if (image->dpi == 0.0)
                                        break;

                                switch (unit)
                                {
                                case ASPECT_RATIO:
                                        image->dpi = 0.0;
                                        break;

                                case DOTS_PER_INCH:
                                        break;

                                case DOTS_PER_CM:
                                        image->dpi *= (float) 2.54;
                                        break;

                                default:
                                        fprintf(stderr, "Warning: JPEG file contains unknown JFIF resolution unit - ignored!\n");
                                        image->dpi = 0;
                                        break;
                                }
                        }
                        break;

                /* ignore markers without parameters */
                case M_SOI:
                case M_EOI:
                case M_TEM:
                case M_RST0:
                case M_RST1:
                case M_RST2:
                case M_RST3:
                case M_RST4:
                case M_RST5:
                case M_RST6:
                case M_RST7:
                        break;

                /* skip variable length markers */
                default:
                        length = getshort(image->fp);
                        for (length -= 2; length > 0; length--)
                                (void) getc(image->fp);
                        break;
                }
        }

        /* do some sanity checks with the parameters */
        if (image->height <= 0 || image->width <= 0 || image->components <= 0)
        {
                fprintf(stderr, "Error: DNL marker not supported in PostScript Level 2!\n");
                return FALSE;
        }

        /* some broken JPEG files have this but they print anyway... */
        if (length != (unsigned int) (image->components * 3 + 8))
                fprintf(stderr, "Warning: SOF marker has incorrect length - ignored!\n");

        if (image->bits_per_component != 8) {
                fprintf(stderr, "Error: %d bits per color component ", image->bits_per_component);
                fprintf(stderr, "not supported in PostScript level 2!\n");
                return FALSE;
        }

        if (image->components!=1 && image->components!=3 && image->components!=4) {
                fprintf(stderr, "Error: unknown color space (%d components)!\n", image->components);
                return FALSE;
        }

        fseek(image->fp, 0, SEEK_END);
        image->filesize = ftell(image->fp);
        fseek(image->fp, 0, SEEK_SET);

        return TRUE;
}

static int JPEGtoPS(imagedata * JPEG, FILE * PSfile)
{
        float xscale, yscale, scale;
        char buf[4096];
        int n;
        int w, h;
        int pw, ph;

        /* read image parameters and fill JPEG struct */
        if (!scanjpeg(JPEG)) {
                fprintf(stderr, "Error: '%s' is not a proper JPEG file!\n", JPEG->filename);
                return;
        }

        fprintf(stderr,
                "%s: %dx%d %s ",
                        JPEG->filename, JPEG->width, JPEG->height,
                        (JPEG->components == 1 ? "gray" : "rgb"));

        if (center) {
            if (JPEG->width < JPEG->height) {
                    printf("portrait ");
                    pw = pagew;
                    ph = pageh;
            }
            else {
                    printf("landscape ");
                    pw = pageh;
                    ph = pagew;
            }

            xscale = (float) (pw - margin * 2) / JPEG->width;
            yscale = (float) (ph - margin * 2) / JPEG->height;

            scale = xscale < yscale ? xscale : yscale;

            printf("%g\n", scale);

            w = JPEG->width * scale;
            h = JPEG->height * scale;
        }
        else {
            if (JPEG->width < JPEG->height) {
                    printf("portrait ");
                    pw = JPEG->width * 72 / dpi;
                    ph = JPEG->height * 72 / dpi;
            }
            else {
                    printf("landscape ");
                    pw = JPEG->height * 72 / dpi;
                    ph = JPEG->width * 72 / dpi;
            }

            printf("[%d,%d]\n", pw, ph);

            w = JPEG->width * 72 / dpi;
            h = JPEG->height * 72 / dpi;
        }

        xref[nxref++] = ftell(PSfile);
        fprintf(PSfile, "%d 0 obj\n", nxref);
        fprintf(PSfile, "<</Type/XObject/Subtype/Image\n");
        fprintf(PSfile, "/Width %d /Height %d\n", JPEG->width, JPEG->height);
        fprintf(PSfile, "/ColorSpace/%s\n", JPEG->components == 1 ? "DeviceGray" : "DeviceRGB");
        fprintf(PSfile, "/BitsPerComponent %d\n", JPEG->bits_per_component);
        fprintf(PSfile, "/Length %d\n", JPEG->filesize);
        fprintf(PSfile, "/Filter/DCTDecode\n");
        fprintf(PSfile, ">>\n");
        fprintf(PSfile, "stream\n");

        /* copy data from jpeg file */
        fseek(JPEG->fp, 0, SEEK_SET);
        while ((n = fread(buf, 1, sizeof(buf), JPEG->fp)) != 0)
                fwrite(buf, 1, n, PSfile);

        fprintf(PSfile, "endstream\n");
        fprintf(PSfile, "endobj\n");
        fprintf(PSfile, "\n");

        sprintf(buf, "%d 0 0 %d %d %d cm /x%d Do\n",
                w, h, (pw-w)/2, (ph-h)/2, nxref);

        xref[nxref++] = ftell(PSfile);
        fprintf(PSfile, "%d 0 obj\n<</Length %d>>\n", nxref, strlen(buf));
        fprintf(PSfile, "stream\n");
        fprintf(PSfile, "%s", buf);
        fprintf(PSfile, "endstream\n");
        fprintf(PSfile, "endobj\n");
        fprintf(PSfile, "\n");

        xref[nxref++] = ftell(PSfile);
        fprintf(PSfile, "%d 0 obj\n", nxref);
        fprintf(PSfile, "<</Type/Page/Parent 3 0 R\n");
        fprintf(PSfile, "/Resources << /XObject << /x%d %d 0 R >> >>\n", nxref-2, nxref-2);
        fprintf(PSfile, "/MediaBox [0 0 %d %d]\n", pw, ph);
        fprintf(PSfile, "/Contents %d 0 R\n", nxref-1);
        fprintf(PSfile, ">>\n");
        fprintf(PSfile, "endobj\n");
        fprintf(PSfile, "\n");

        return nxref;
}

void usage(void)
{
        fprintf(stderr, "usage: tocomicpdf -o outfile.pdf -t 'title' [files.jpg]\n");
        exit(2);
}

int main(int argc, char **argv)
{
        imagedata image;
        FILE *outfile;
        int i;
        int startxref;
        char *outname = "out.pdf";
        char *title = "(Untitled)";
        int c;

        while ((c = getopt(argc, argv, "o:t:fh:w:m:d:")) != -1)
        {
                switch (c)
                {
                case 'o':
                        outname = optarg;
                        break;
                case 't':
                        title = optarg;
                        break;
                case 'f':
                        center = 0;
                        break;
                case 'h':
                        pageh = atoi(optarg) / 10.0f / 2.54f * 72.0f;
                        break;
                case 'w':
                        pagew = atoi(optarg) / 10.0f / 2.54f * 72.0f;
                        break;
                case 'm':
                        margin = atoi(optarg) / 10.0f / 2.54f * 72.0f;
                        break;
                case 'd':
                        dpi = atoi(optarg);
                        break;
                default:
                        usage();
                        break;
                }
        }

        if (center)
            printf("Centering images on pages\n");
        else
            printf("Fitting pages to images\n");

        image.filename = NULL;

        outfile = fopen(outname, "w");

        fprintf(outfile, "%%PDF-1.3\n\n");

        xref[nxref++] = ftell(outfile);
        fprintf(outfile, "1 0 obj\n");
        fprintf(outfile, "<</Type/Catalog/Pages 3 0 R>>\n");
        fprintf(outfile, "endobj\n\n");

        xref[nxref++] = ftell(outfile);
        fprintf(outfile, "2 0 obj\n");
        fprintf(outfile, "<</Creator(jpegtopdf)/Title(%s)>>\n", title);
        fprintf(outfile, "endobj\n\n");

        /* delay obj #3 (pages) until later */
        nxref++;

        for (i = optind; i < argc; i++) {
                image.filename = argv[i];

                if ((image.fp = fopen(image.filename, "r")) == NULL) {
                        fprintf(stderr, "Error: couldn't read JPEG file '%s'!\n",
                                image.filename), exit(1);
                }

                pages[npages++] = JPEGtoPS(&image, outfile);    /* convert JPEG data */

                fclose(image.fp);
        }

        xref[2] = ftell(outfile);
        fprintf(outfile, "3 0 obj\n");
        fprintf(outfile, "<</Type/Pages/Count %d/Kids[\n", npages);
        for (i = 0; i < npages; i++)
                fprintf(outfile, "%d 0 R\n", pages[i]);
        fprintf(outfile, "]>>\nendobj\n\n");

        startxref = ftell(outfile);
        fprintf(outfile, "xref\n0 %d\n", nxref + 1);
        fprintf(outfile, "0000000000 65535 f \n");
        for (i = 0; i < nxref; i++)
                fprintf(outfile, "%0.10d 00000 n \n", xref[i]);
        fprintf(outfile, "trailer\n<< /Size %d /Root 1 0 R /Info 2 0 R >>\n", nxref + 1);
        fprintf(outfile, "startxref\n%d\n%%%%EOF\n", startxref);

        fclose(outfile);

        return 0;
}
