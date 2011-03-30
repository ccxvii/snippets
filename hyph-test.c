#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utf.h"
#include "hyph.h"

extern int memuse, numnodes;
int numpats;
int mempat;

int
main(int argc, char **argv)
{
	char buf[256];
	Rune word[256];
	char vals[256];
	TrieNode *hyph;
	char *s;
	int k;
	FILE *f;
	Rune patstr[256];
	char patvalbuf[256];
	char *patval;
	int patlen;

	memuse = 0;
	mempat = 0;

	puts("Loading hyph trie...");
	strcpy(buf, "hyphen.tex");
	if (argc > 1)
		strcpy(buf, argv[1]);

	hyph = malloc(sizeof (TrieNode));
	hyph->patlen = 0;
	hyph->patval = NULL;
	hyph->child = NULL;
	hyph->next = NULL;

	f = fopen(buf, "r");
	if (!f) { perror(buf); exit(1); }
	while (1) {
		fgets(buf, 256, f);
		if (feof(f)) break;
		buf[strlen(buf)-1] = '\0';
		numpats ++;
		patlen = hyph_makepattern(buf, patstr, patvalbuf);
		patval = malloc(patlen+1);
		memcpy(patval, patvalbuf, patlen+1);
		mempat += patlen + 1;
		hyph_integratepattern(hyph, patstr, patval, patlen, 0);
	}
	fclose(f);

	printf("sizeof node = %d\n", sizeof (TrieNode));
	printf("Done [%d+%d bytes used; %d nodes; %d patterns].\n", memuse, mempat, numnodes, numpats);
//	printf("nt=%d na=%d np=%d\n", numtrie, numarc, numpat);
	printf("Type word to hyphenate:\n");

	while (!feof(stdin)) {
		fgets(buf, 256, stdin);
		if (feof(stdin))
			break;
		buf[strlen(buf)-1] = '\0';
		if (strcmp(buf, ".") == 0)
			break;

		/* build word to hyphenate, surround with '.' and strip illegal chars */
		k = 0;
		word[k++] = '.';
		s = buf;
		do {
			s += chartorune(&word[k++], s);
		} while (*s != '\0');
		word[k++] = '.';
		hyph_hyphenate(hyph, word, vals, k);
	}

	hyph_freetrie(hyph);

	return 0;
}
