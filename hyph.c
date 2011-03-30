#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "utf.h"
#include "hyph.h"

#define MAX(a,b)			((a) > (b) ? (a) : (b))

int memuse = 0;
int numnodes = 0;
void* mymalloc(int size) { memuse += size; return malloc(size); }

void
hyph_integratepattern(TrieNode *trie, Rune *patstr, char *patval, int patlen, int idx)
{
	TrieNode *arc;

	if (idx >= patlen) {
		trie->patlen = patlen;
		trie->patval = patval;
	}
	else {
		for (arc = trie->child; arc; arc = arc->next)
			if (arc->ch == patstr[idx])
				break;
		if (!arc) {
numnodes ++;
			arc = mymalloc(sizeof (TrieNode));
			arc->ch = patstr[idx];
			arc->patlen = 0;
			arc->patval = NULL;
			arc->child = NULL;
			arc->next = trie->child;
			trie->child = arc;
		}
		hyph_integratepattern(arc, patstr, patval, patlen, idx+1);
	}
}

void
hyph_freetrie(TrieNode *trie)
{
	if (trie->patval)
		free(trie->patval);
	if (trie->child)
		hyph_freetrie(trie->child);
	if (trie->next)
		hyph_freetrie(trie->next);
	free(trie);
}

int
hyph_makepattern(char *s0, Rune *patstr, char *patval)
{
	char *s;
	Rune r;
	int k, i;
	int patlen;

	/* count number of chars in pattern */
	patlen = 0;
	s = s0;
	do {
		s += chartorune(&r, s);
		if (!isdigit(r))
			patlen ++;
	} while (*s != '\0');

	/* zero values */
	for (i = 0; i < patlen + 1; i++)
		patval[i] = 0;

	/* extract interleaved values and chars */
	k = 0;
	s = s0;
	do {
		s += chartorune(&r, s);
		if (isdigit(r)) {
			patval[k] *= 10;
			patval[k] += r - '0';
		}
		else {
			patstr[k] = r;
			k ++;
		}
	} while (*s != '\0');

	return patlen;
}

void
hyph_hyphenate(TrieNode *trie, Rune *word, char *vals, int len)
{
	int i, j, k;
	TrieNode *node, *m;

	/* clear value buffer */
	for (i = 0; i < len + 1; i++)
		vals[i] = 0;

	/* for all chars in word */
	for (i = 0; i < len; i++)
	{
		/* follow trie finding matching pattern */
		node = trie;
		j = 0;
		while (node && i + j < len + 1)
		{
			/* copy pattern values we have far */
			if (node->patval) {
				for (k = 0; k < node->patlen + 1; k++) {
					vals[i+k] = MAX(vals[i+k], node->patval[k]);
				}
			}

			/* find next trie node for current char */
			m = node->child;
			node = NULL;
			while (m) {
				if (m->ch == word[i+j]) {
					node = m;
					break;
				}
				m = m->next;
			}

			/* next char */
			j ++;
		}
	}

	/* print hyphenated word */
	for (i = 1; i < len-1; i++) {
		if (vals[i] % 2 == 1 && i > 2 && i < len - 2) {
			putchar('+');
		}
		putchar(word[i]);
	}
	putchar('\n');
}
