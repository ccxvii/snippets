/*
 * $Id$
 *
 * Optimal paragraph line-breaker
 *
 * atoms and moles
 * a line can be broken at any mole where short <= maxlen && long >= maxlen
 * try all these breaks and find best total fit for para
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <strings.h>

enum { GLYPH, SPACE };
enum { ATOM, MOLE };
enum { AWHOLE=0, APRE=1, APOST=2 };

typedef struct {
    int type; /* GLYPH, SPACE */
    int code;
    float width;
    // XXX font ?
} quark_t;

typedef struct {
    int idx, len; /* idx + len in quark list */
    float sw, nw, lw; /* short/ideal/long width of group */
    // XXX shrink/stretch penalty scale
} atom_t;

typedef struct {
    int type; /* ATOM, MOLE */
    atom_t a[3]; /* whole, pre, post */
    // XXX penalty

    int prev; /* previous break */
    int next; /* next break (set at end of formatting) */
    int demerits; /* accumulated cost for breaking here */
} item_t;

/* ----------------------------------------------------------------------- */

static int nquarks;
static quark_t quark[20000];
static int nitems;
static item_t item[20000];

static int idx = 0;
static int len = 0;

void para_add_quark(int t, int c, float w)
{
    quark[nquarks].type = t;
    quark[nquarks].code = c;
    quark[nquarks].width = w;
    nquarks++;
    len++;
}

void para_add_atom(int i)
{
    item[nitems].a[i].idx = idx;
    item[nitems].a[i].len = len;

    item[nitems].a[i].sw = len;
    item[nitems].a[i].nw = len;
    item[nitems].a[i].lw = len;

    idx = nquarks;
    len = 0;
}

void para_add_item(int t)
{
    item[nitems].type = t;
    nitems++;
}

void para_add_sentinel(void)
{
    item[nitems].type = MOLE;
}

void para_add_root(void)
{
    int i;
    for (i=0; i < 3; i++) {
        item[nitems].a[i].idx = 0;
        item[nitems].a[i].len = 0;
        item[nitems].a[i].sw = 0;
        item[nitems].a[i].nw = 0;
        item[nitems].a[i].lw = 0;
    }
    item[nitems].type = MOLE;
    nitems++;
}

/* ----------------------------------------------------------------------- */

#define MINWIDTH 50
#define MAXWIDTH 75

static int brkbuf[10000];
static int brkbest[10000];
static int nbrkbest;
static int bestcost;

int para_cost(int start, int end, float sw, int depth)
{
    return depth * 10 + (MAXWIDTH-sw)*(MAXWIDTH-sw)*30;
}

void para_format(int start, int accum, int depth)
{
    float sw, lw;
    int cost;
    int i, k;

    for (i=0; i < depth; i++) putchar(' ');
    printf("fmt: %d %d %d\n", start, accum, depth);

    /* at least one word per line... */
    i = start + 1;
    sw = item[start].a[0].sw;
    lw = item[start].a[0].lw;

    /* explore all potential breaks */
    while (sw <= MAXWIDTH && i < nitems)
    {
        /* potential break... explore */
        if (item[i].type == MOLE && lw >= MINWIDTH)
        {
            brkbuf[depth] = i;
            cost = accum + para_cost(start, i, sw, depth);

            /* not worth trying if already past bestcost */
            if (cost < bestcost) {
                para_format(i, cost, depth+1);
            }
        }

        /* ... or don't */
        sw += item[i].a[AWHOLE].sw;
        lw += item[i].a[AWHOLE].lw;
        i++;
    }

    /* hit end... keep if better than the best */
    if (i == nitems)
    {
        printf("hit end with cost: %d ... %d\n", accum, bestcost);
        if (accum < bestcost) {
            printf("better sequence: %d\n", accum);
            bestcost = accum;
            for (k=0; k < depth; k++) {
                brkbest[k] = brkbuf[k];
                printf("%d ", brkbest[k]);
            }
            printf("\n");
            nbrkbest = depth;
            //pretty();
        }
        return;
    }
}

/* ----------------------------------------------------------------------- */

void para_format2(void)
{
    int cur, child, first, demerits;
    float wid;

    for (cur=0; cur < nitems+1 ; cur++) {
        item[cur].demerits = 0;
        item[cur].prev = -1;
        item[cur].next = -1;
    }

    /* consider all possible breaks */
    for (cur = 0 ; cur < nitems ; cur++)
    {
        printf("fmt2: %d/%d\n", cur, nitems);

        /* skip unbreakables */
        if (item[cur].type == ATOM) {
            continue;
        }

        /* try to make a line as long as possible starting after cur */
        child = cur + 1;
        wid = 0; /* cur[APOST] */

        /* keep going until it is too long */
        while (wid < MAXWIDTH && child <= nitems)
        {
            /* no more items */
            if (child == nitems) {
                demerits = item[cur].demerits;
                demerits += 10;
                printf("  drop = %d\n", demerits);
            }

            /* potential break: save demerits if broken here */
            else if (item[child].type == MOLE) {
                demerits = item[cur].demerits;
                //demerits += (MAXWIDTH - wid) * (MAXWIDTH - wid) * 30;
                demerits += 10; /* Line penalty */
                printf("  child[%d] = %d\n", child, demerits);
            }

            /* potential break (sentinel or mole) */
            if (child == nitems || item[child].type == MOLE)
            {
                /* check if we have a better path to get to child */
                if (item[child].prev == -1 || demerits < item[child].demerits)
                {
                    printf("  -> saved[%d].prev = %d\n", child, cur);
                    item[child].prev = cur;
                    item[child].demerits = demerits;
                }
            }

            /* keep track of widths */
            wid += item[child].a[0].sw;

            /* move to next potential item */
            child ++;
        }
    }

    printf("---\n");
    for (cur=0; cur < nitems; cur++) {
        if (item[cur].type == MOLE) {
            printf("item[%d] prev=%d demerits=%d\n",
                   cur, item[cur].prev, item[cur].demerits);
        }
    }
    printf("---\n");

    printf("recovering...\n");

    /* recover best path */
    for (cur = nitems; cur != -1; cur = item[cur].prev) {
        printf("cur=%d prev=%d\n", cur, item[cur].prev);
        if (item[cur].prev == -1) {
            first = cur;
        }
        else {
            item[item[cur].prev].next = cur;
        }
    }

    printf("storing...\n");

    /* and store it */
    nbrkbest = 0;
    for (cur = first ; cur != -1 ; cur = item[cur].next) {
        brkbest[nbrkbest++] = cur;
        printf("%d: %d\n", nbrkbest-1, cur);
    }
}

/* ----------------------------------------------------------------------- */

void parse(char *s)
{
    char *w = strtok(s, " ");
    while (w != NULL) {
        while (*w != '\0') {
            para_add_quark(GLYPH, *w, 1);
            w++;
        }
        para_add_atom(AWHOLE);
        para_add_item(ATOM);

        para_add_quark(SPACE, ' ', 1);
        para_add_atom(AWHOLE);
        para_add_atom(APRE);
        para_add_atom(APOST);
        para_add_item(MOLE);

        w = strtok(NULL, " ");
    }

    nitems--;
}

void print(void)
{
    int i, k;
    for (i = 0 ; i < nitems ; i++) {
        printf("mol[%d/%d]: ", i, item[i].type);
        for (k = item[i].a[0].idx; k < item[i].a[0].idx+item[i].a[0].len; k++)
        {
            putchar(quark[k].code);
        }
        putchar('\n');
    }
}

void pretty(void)
{
    int i, k, l;
    l = 0;
    for (i = 0 ; i < nitems ; i++) {
        for (k = item[i].a[0].idx; k < item[i].a[0].idx+item[i].a[0].len; k++)
        {
            putchar(quark[k].code);
        }
        if (brkbest[l] == i && l < nbrkbest) {
            putchar('\n');
            l++;
        }
    }
    putchar('\n');
}

char text[] =
"In the population of Transylvania there are four "
"distinct nationalities: Saxons in the South, "
"and mixed with them the Wallachs, who are the "
"descendants of the Dacians; Magyars in the West, "
"and Szekelys in the East and North. I am going "
"among the latter, who claim to be descended from "
"Attila and the Huns. This may be so, for when "
"the Magyars conquered the country in the eleventh "
"century they found the Huns settled in it."
/*
*/
"In the population of Transylvania there are four "
"distinct nationalities: Saxons in the South, "
"and mixed with them the Wallachs, who are the "
"descendants of the Dacians; Magyars in the West, "
"and Szekelys in the East and North. I am going "
"among the latter, who claim to be descended from "
"Attila and the Huns. This may be so, for when "
"the Magyars conquered the country in the eleventh "
"century they found the Huns settled in it."
/*
*/
"In the population of Transylvania there are four "
"distinct nationalities: Saxons in the South, "
"and mixed with them the Wallachs, who are the "
"descendants of the Dacians; Magyars in the West, "
"and Szekelys in the East and North. I am going "
"among the latter, who claim to be descended from "
"Attila and the Huns. This may be so, for when "
"the Magyars conquered the country in the eleventh "
"century they found the Huns settled in it."
/*
*/
"In the population of Transylvania there are four "
"distinct nationalities: Saxons in the South, "
"and mixed with them the Wallachs, who are the "
"descendants of the Dacians; Magyars in the West, "
"and Szekelys in the East and North. I am going "
"among the latter, who claim to be descended from "
"Attila and the Huns. This may be so, for when "
"the Magyars conquered the country in the eleventh "
"century they found the Huns settled in it."
;

int main(int argc, char **argv)
{
    para_add_root();
    parse(text);
    para_add_sentinel();

    print();

    bestcost = 1<<30;
    //para_format(0,0,0);
    //pretty();

    bestcost = 1<<30;
    para_format2();
    pretty();
}
