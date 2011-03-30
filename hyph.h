typedef struct TrieNode		TrieNode;

struct TrieNode
{
	char			*patval;	/* nil if not leaf */
	short		patlen;	/* num values - 1 in pattern */
	Rune		ch;		/* char to branch on (not used for leafs) */
	TrieNode	*child;
	TrieNode	*next;
};

/* hyph.c */
int hyph_makepattern(char *s, Rune *patstr, char *patval);
void hyph_integratepattern(TrieNode *trie, Rune *patstr, char *patval, int patlen, int idx);
void hyph_hyphenate(TrieNode *trie, Rune *s, char *v, int len);
void hyph_freetrie(TrieNode *trie);
