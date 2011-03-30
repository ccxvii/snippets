/* Linear probe hash table.
 * 2004 (C) Tor Andersson.
 * BSD license.
 *
 * Simple hashtable with open adressing linear probe.
 * Unlike text book examples, removing entries works
 * correctly in this implementation so it wont start
 * exhibiting bad behaviour if entries are inserted
 * and removed frequently.
 */ 

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const unsigned primes[] = {
	31, 61, 127, 251, 509, 1021, 2039, 4093, 8191, 16381, 32749, 65521,
	131071, 262139, 524287, 1048573, 2097143, 4194301, 8388593, 0
};

// enum { KEYLEN = 16, SIZE = 509 };
enum { KEYLEN = 4, SIZE = 8 };

typedef struct Hashtable Hashtable;
typedef struct Hashentry Hashentry;

struct Hashentry
{
	char key[KEYLEN];
	void *val;
};

struct Hashtable
{
	unsigned size;
	unsigned load;
	Hashentry ents[SIZE];
};

#if 0
static unsigned hash(char *s)
{
	char *e = s + KEYLEN;
	unsigned h = 0;
	while (s < e)
		h = *s++ + (h << 6) + (h << 16) - h;
	return h;
}
#else
static unsigned hash(char *s)
{
	unsigned i = *(unsigned*)s;
	return i % SIZE;
}
#endif

void hashinit(Hashtable *table)
{
	table->size = SIZE;
	table->load = 0;
	memset(table->ents, 0, sizeof table->ents);
}

void *hashfind(Hashtable *table, char *key)
{
	Hashentry *ents = table->ents;
	unsigned size = table->size;
	unsigned pos = hash(key) % size;

	while (1)
	{
		if (!ents[pos].val)
			return NULL;

		if (memcmp(key, &ents[pos].key, KEYLEN) == 0)
			return ents[pos].val;

		pos = (pos + 1) % size;
	}
}

/*
 * There must be space left (table->load < table->size)
 */
void hashinsert(Hashtable *table, char *key, void *val)
{
	Hashentry *ents = table->ents;
	unsigned size = table->size;
	unsigned pos = hash(key) % size;

	/* if (table->load > table->size * 8 / 10) hashdouble(table); */

	while (1)
	{
		if (!ents[pos].val)
		{
			memcpy(ents[pos].key, key, KEYLEN);
			ents[pos].val = val;
			table->load ++;
			return;
		}

		if (memcmp(key, &ents[pos].key, KEYLEN) == 0)
			return;

		pos = (pos + 1) % size;
	}
}

#define MODSUB(size, look, code) \
	(look >= code) ? look - code : size - (code - look)

void hashremove(Hashtable *table, char *key)
{
	Hashentry *ents = table->ents;
	unsigned size = table->size;
	unsigned pos = hash(key) % size;
	unsigned hole, look, code;

	while (1)
	{
		if (!ents[pos].val)
			return;

		if (memcmp(key, &ents[pos].key, KEYLEN) == 0)
		{
			ents[pos].val = NULL;

			hole = pos;
			look = (hole + 1) % size;

			while (ents[look].val)
			{
				code = hash(ents[look].key) % size;
#if 0
				if (MODSUB(table->size, look, code) >=
					MODSUB(table->size, look, hole))
#else
				if ((code <= hole && hole < look) ||
					(look < code && code <= hole) ||
					(hole < look && look < code))
#endif
				{
					ents[hole] = ents[look];
					ents[look].val = NULL;
					hole = look;
				}

				look = (look + 1) % size;
			}

			table->load --;
			return;
		}

		pos = (pos + 1) % size;
	}
}

void hashdebug(Hashtable *table)
{
	int i;

	printf("cache load %d / %d\n", table->load, table->size);

	for (i = 0; i < table->size; i++)
		if (!table->ents[i].val)
			printf("table % 4d: empty\n", i);
		else
			//printf("table % 4d: key=%-16.16s val=%s\n", i,
				//	table->ents[i].key, (char*)table->ents[i].val);
			printf("table % 4d: key=%d val=%s\n", i,
					*(unsigned*)table->ents[i].key, (char*)table->ents[i].val);
}

int
main(int argc, char **argv)
{
	Hashtable gtable, *table = &gtable;
	unsigned keys[] = { 8, 2, 15, 7, 1 };

	hashinit(table);

	printf("GDATC 71 68 65 84 67\n");
	hashinsert(table, (char*)(keys+0), "G");
	hashinsert(table, (char*)(keys+1), "D");
	hashinsert(table, (char*)(keys+2), "A");
	hashinsert(table, (char*)(keys+3), "T");
	hashinsert(table, (char*)(keys+4), "C");

	hashdebug(table);
	printf("\n");

	hashremove(table, (char*)(keys+2));

	hashdebug(table);
	printf("\n");

	hashremove(table, (char*)(keys+4));

	hashdebug(table);
	printf("\n");

#if 0
	hashinsert(table, "gandalf", "the gray");
	hashinsert(table, "cugel", "the clever");
	hashinsert(table, "rhialto", "the magnificent");
	hashinsert(table, "buffy", "the vampire slayer");
	hashinsert(table, "faith",
		"I know Faith's not gonna be on the cover of Sanity Fair");
	hashinsert(table, "spike", "wants a buffy-bot");

	hashremove(table, "faith");
	hashremove(table, "spike");

	hashinsert(table, "captain", "jean-luc picard");
	hashinsert(table, "counselor", "deanna troi");
	hashinsert(table, "commander", "william riker");
	hashinsert(table, "security officer", "tasha yar (rip)");
	hashinsert(table, "spoiled brat", "wesley crusher");
	hashinsert(table, "annoying", "lwaxana troi");

	hashdebug(table);
#endif

	return 0;
}

