#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <zlib.h>

/* A4: 595 x 842 */
enum
{
    PAGEW = 595,
    PAGEH = 842,
    MARGIN = 36
};

enum
{
    PPM, PGM, PBM
};

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
    FILE *fp;
    char *filename;
    int filesize;
    int width;
    int height;
    int components;
    int bits_per_component;
    float dpi;
} imagedata;

static void readwhitespace(FILE *f, char *filename, int max)
{
    int done = 0;
    int c = 0;
    int count = 0;

    do {
        c = fgetc(f);
        if (c == EOF)
            perror(filename);
        count++;

        if (c != ' ' && c != '\t' && c != '\r' && c != '\n') {
            c = ungetc(c, f);
            if (c == EOF)
                perror(filename);
            done = 1;
        }

        if (max != 0 && count == max)
            done = 1;

    } while (!done);
}

static int readnumber(FILE *f, char *filename)
{
    int done = 0;
    int c = 0;
    int count = 0;
    unsigned int number = 0;
    char digits[11];

    memset(digits, '\0', sizeof digits);

    do {
        c = fgetc(f);
        if (c == EOF)
            perror(filename);

        if (c < '0' || c > '9' || count == 10) {
            c = ungetc(c, f);
            if (c == EOF)
                perror(filename);
            done = 1;
        } else {
            digits[count++] = c;
        }

    } while (!done);

    number = atoi(digits);

    return number;
}

static int readtobuf(FILE *f, char *filename, unsigned char *buf, unsigned int len)
{
    size_t total = 0;

    do {
        size_t read = fread(&buf[total], 1, len - total, f);
        if (read != len - total && ferror(f))
            perror(filename);
        if (read != len - total && feof(f))
            return total + read;

        total += read;
    } while (total < len);

    return total;
}

static int writefrombuf(FILE *f, char *filename, unsigned char *buf, unsigned int len)
{
    size_t total = 0;

    do {
        size_t wrote = fwrite(&buf[total], 1, len - total, f);
        if (wrote != len - total && ferror(f))
            perror(filename);

        total += wrote;
    } while (total < len);

    return total;
}

static int flateimage(imagedata *image, FILE *pdf)
{
    char data[16384];
    int dataused;
    char flated[16384];
    int flateused;
    int err;
    z_stream zstate;
    int inputdone, flatedone;

	memset(&zstate, 0, sizeof zstate);

    err = deflateInit(&zstate, 9);
    if (err != Z_OK)
    {
        fprintf(stderr, "deflateInit() returned %d\n", err);
        exit(1);
    }

    flateused = 0;
    dataused = 0;
    inputdone = 0;
    flatedone = 0;

    while (1)
    {
        int read;

        if (!inputdone)
        {
            read = readtobuf(image->fp, image->filename, data, sizeof data - dataused);
            if (read == 0)
                inputdone = 1;

            dataused += read;

            zstate.next_in = data;
            zstate.avail_in = dataused;
        }

        zstate.next_out = flated;
        zstate.avail_out = sizeof flated;

        err = deflate(&zstate, inputdone ? Z_FINISH : 0);
        if (err == Z_STREAM_END)
        {
            flatedone = 1;
        }
        else if (err != Z_OK)
        {
            fprintf(stderr, "deflate() returned %d\n", err);
            exit(1);
        }

        flateused = sizeof flated - zstate.avail_out;
        writefrombuf(pdf, "out.pdf", flated, flateused);
        flateused = 0;

        if (inputdone && flatedone)
            break;

        memmove(data, &data[dataused - zstate.avail_in], zstate.avail_in);
        dataused = zstate.avail_in;
    }

    err = deflateEnd(&zstate);
    if (err != Z_OK)
    {
        fprintf(stderr, "deflateEnd() returned %d\n", err);
        exit(1);
    }

    return zstate.total_out;
}

static int pnmtops(imagedata *image, FILE *pdf, char *outname)
{
    char buf[4096];
    float xscale;
    float yscale;
    float scale;
    int imgw, imgh, imgc;
    int w, h;
    int pw, ph;
    int bpc;
    int len;
    int total;
    int maxval;
    struct stat statinfo;
    int format;
    int fd;
    int curr;
    int flated;

    total = 0;
    do {
        int read = fread(&buf[total], 1, 2 - total, image->fp);
        if (read != 2 - total && ferror(image->fp))
            perror(image->filename);

        total += read;
    } while (total < 2);

    if (memcmp(buf, "P6", 2) == 0)
        format = PPM;
    else if (memcmp(buf, "P5", 2) == 0)
        format = PGM;
    else if (memcmp(buf, "P4", 2) == 0)
        format = PBM;
    else {
        fprintf(stderr, "Unknown file format '%c%c' in file '%s'\n", buf[0], buf[1], image->filename);
        exit(1);
    }

    readwhitespace(image->fp, image->filename, 0);
    imgw = readnumber(image->fp, image->filename);
    readwhitespace(image->fp, image->filename, 0);
    imgh = readnumber(image->fp, image->filename);

    if (format == PPM || format == PGM) {
        readwhitespace(image->fp, image->filename, 0);
        maxval = readnumber(image->fp, image->filename);
    }

    readwhitespace(image->fp, image->filename, 1);

    fd = fileno(image->fp);
    if (fd == -1)
        perror(image->filename);

    if (fstat(fd, &statinfo) == -1)
        perror(image->filename);

    curr = ftell(image->fp);
    if (curr == -1)
        perror(image->filename);

    len = statinfo.st_size - curr;

    switch (format) 
    {
        case PPM:
            imgc = 3;
            bpc = (unsigned int) ceil(log(maxval)/log(2));
            break;

        case PGM:
            imgc = 1;
            bpc = (unsigned int) ceil(log(maxval)/log(2));
            break;

        case PBM:
            imgc = 1;
            bpc = 1;
            break;
    }

    fprintf(stderr, "%s: %dx%d %s ",
            image->filename, imgw, imgh,
            (imgc == 1 ? "gray" : "rgb"));

    if (center) {
        if (imgw < imgh) {
            fprintf(stderr, "portrait ");
            pw = pagew;
            ph = pageh;
        }
        else {
            fprintf(stderr, "landscape ");
            pw = pageh;
            ph = pagew;
        }

        xscale = (float) (pw - margin * 2) / imgw;
        yscale = (float) (ph - margin * 2) / imgh;

        scale = xscale < yscale ? xscale : yscale;

        fprintf(stderr, "%g\n", scale);

        w = imgw * scale;
        h = imgh * scale;
    }
    else {
        pw = imgw * 72 / dpi;
        ph = imgh * 72 / dpi;

        fprintf(stderr, "[%d,%d]\n", pw, ph);

        w = imgw * 72 / dpi;
        h = imgh * 72 / dpi;
    }

    xref[nxref++] = ftell(pdf);
    fprintf(pdf, "%d 0 obj\n", nxref);
    fprintf(pdf, "<</Type/XObject/Subtype/Image\n");
    fprintf(pdf, "/Width %d /Height %d\n", imgw, imgh);
    fprintf(pdf, "/ColorSpace/%s\n", format == PPM ? "DeviceRGB" : "DeviceGray");
    fprintf(pdf, "/BitsPerComponent %d\n", bpc);
    fprintf(pdf, "/Filter /FlateDecode\n");
    fprintf(pdf, "/Length %d 0 R\n", nxref + 1);
    fprintf(pdf, ">>\n");
    fprintf(pdf, "stream\n");

    flated = flateimage(image, pdf);

    fprintf(pdf, "endstream\n");
    fprintf(pdf, "endobj\n\n");

    xref[nxref++] = ftell(pdf);
    fprintf(pdf, "%d 0 obj\n", nxref);
    fprintf(pdf, "%d\n", flated);
    fprintf(pdf, "endobj\n\n");

    sprintf(buf, "%d 0 0 %d %d %d cm /x%d Do\n",
            w, h,
            (pw - w) / 2, (ph - h) / 2,
            nxref - 1);

    xref[nxref++] = ftell(pdf);
    fprintf(pdf, "%d 0 obj\n<</Length %d>>\n", nxref, strlen(buf));
    fprintf(pdf, "stream\n");
    fprintf(pdf, "%s", buf);
    fprintf(pdf, "endstream\n");
    fprintf(pdf, "endobj\n\n");

    xref[nxref++] = ftell(pdf);
    fprintf(pdf, "%d 0 obj\n", nxref);
    fprintf(pdf, "<</Type/Page/Parent 3 0 R\n");
    fprintf(pdf, "/Resources << /XObject << /x%d %d 0 R >> >>\n",
            nxref - 3, nxref - 3);
    fprintf(pdf, "/MediaBox [0 0 %d %d]\n", pw, ph);
    fprintf(pdf, "/Contents %d 0 R\n", nxref - 1);
    fprintf(pdf, ">>\n");
    fprintf(pdf, "endobj\n\n");

    return nxref;
}


void usage()
{
    fprintf(stderr, "usage: pnmtopdf -o outfile.pdf -t 'title' [files.jpg]\n");
    exit(2);
}
    

int main(int argc, char **argv)
{
    imagedata image;
    FILE *outfile;
    int i;
    int startxref;
    char *outname = "out.pdf";
    char *title = "Untitled";
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
                pageh = atoi(optarg) / 10.f / 2.54f * 72.0f;
                break;
            case 'w':
                pagew = atoi(optarg) / 10.f / 2.54f * 72.0f;
                break;
            case 'm':
                margin = atoi(optarg) / 10.f / 2.54f * 72.0f;
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
        fprintf(stderr, "Centering images on pages\n");
    else
        fprintf(stderr, "Fitting pages to images\n");

    image.filename = NULL;
    outfile = fopen(outname, "w");

    fprintf(outfile, "%%PDF-1.3\n\n");

    xref[nxref++] = ftell(outfile);
    fprintf(outfile, "1 0 obj\n");
    fprintf(outfile, "<</Type/Catalog/Pages 3 0 R>>\n");
    fprintf(outfile, "endobj\n\n");

    xref[nxref++] = ftell(outfile);
    fprintf(outfile, "2 0 obj\n");
    fprintf(outfile, "<</Creator(pnmtopdf)/Title(%s)>>\n", title);
    fprintf(outfile, "endobj\n\n");

    /* delay obj #3 (pages) until later */
    nxref++;

    for (i = optind; i < argc; i++)
    {
        image.filename = argv[i];

        if ((image.fp = fopen(image.filename, "r")) == NULL)
        {
            fprintf(stderr, "Error: couldn't read pnm file '%s'!\n", image.filename);
            exit(1);
        }

        pages[npages++] = pnmtops(&image, outfile, outname);

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
    fprintf(outfile, "trailer\n<< /Size %d /Root 1 0 R /Info 2 0 R>>\n", nxref + 1);
    fprintf(outfile, "startxref\n%d\n%%%%EOF\n", startxref);

    fclose(outfile);

    return 0;
}
