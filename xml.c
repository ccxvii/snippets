#include <stdlib.h> /* malloc, free, strtol */
#include <string.h> /* memmove, strcmp */

#include "xml.h"

static int runetochar(char *s, int c)
{
	if (c < 0x80) {
		s[0] = c;
		return 1;
	}
	if (c < 0x800) {
		s[0] = 0xC0 | (c >> 6);
		s[1] = 0x80 | (c & 0x3F);
		return 2;
	}
	if (c > 0x10FFFF)
		c = 0xFFFD;
	if (c < 0x1000) {
		s[0] = 0xE0 | (c >> 12);
		s[1] = 0x80 | ((c >> 6) & 0x3F);
		s[2] = 0x80 | (c & 0x3F);
		return 3;
	}
	s[0] = 0xf0 | (c >> 18);
	s[1] = 0x80 | ((c >> 12) & 0x3F);
	s[2] = 0x80 | ((c >> 6) & 0x3F);
	s[3] = 0x80 | (c & 0x3F);
	return 4;
}

struct {
	xml_item *head;
	int preserve_white;
	int depth;
} g;

struct attribute {
	char name[40];
	char *value;
	struct attribute *next;
};

struct xml_item {
	char name[40];
	char *text;
	struct attribute *atts;
	xml_item *up, *down, *tail, *prev, *next;
};

xml_item *xml_prev(xml_item *item)
{
	return item ? item->prev : NULL;
}

xml_item *xml_next(xml_item *item)
{
	return item ? item->next : NULL;
}

xml_item *xml_up(xml_item *item)
{
	return item ? item->up : NULL;
}

xml_item *xml_down(xml_item *item)
{
	return item ? item->down : NULL;
}

char *xml_text(xml_item *item)
{
	return item ? item->text : NULL;
}

char *xml_tag(xml_item *item)
{
	return item && item->name[0] ? item->name : NULL;
}

int xml_is_tag(xml_item *item, const char *name)
{
	if (!item)
		return 0;
	return !strcmp(item->name, name);
}

char *xml_att(xml_item *item, const char *name)
{
	struct attribute *att;
	if (!item)
		return NULL;
	for (att = item->atts; att; att = att->next)
		if (!strcmp(att->name, name))
			return att->value;
	return NULL;
}

xml_item *xml_find(xml_item *item, const char *tag)
{
	while (item) {
		if (!strcmp(item->name, tag))
			return item;
		item = item->next;
	}
	return NULL;
}

xml_item *xml_find_next(xml_item *item, const char *tag)
{
	if (item)
		item = item->next;
	return xml_find(item, tag);
}

xml_item *xml_find_down(xml_item *item, const char *tag)
{
	if (item)
		item = item->down;
	return xml_find(item, tag);
}

static void xml_free_attribute(struct attribute *att)
{
	while (att) {
		struct attribute *next = att->next;
		if (att->value)
			free(att->value);
		free(att);
		att = next;
	}
}

void xml_free(xml_item *item)
{
	while (item) {
		xml_item *next = item->next;
		if (item->text)
			free(item->text);
		if (item->atts)
			xml_free_attribute(item->atts);
		if (item->down)
			xml_free(item->down);
		free(item);
		item = next;
	}
}

static int xml_parse_entity(int *c, char *a)
{
	char *b;
	if (a[1] == '#') {
		if (a[2] == 'x')
			*c = strtol(a + 3, &b, 16);
		else
			*c = strtol(a + 2, &b, 10);
		if (*b == ';')
			return b - a + 1;
	} else if (a[1] == 'l' && a[2] == 't' && a[3] == ';') {
		*c = '<';
		return 4;
	} else if (a[1] == 'g' && a[2] == 't' && a[3] == ';') {
		*c = '>';
		return 4;
	} else if (a[1] == 'a' && a[2] == 'm' && a[3] == 'p' && a[4] == ';') {
		*c = '&';
		return 5;
	} else if (a[1] == 'a' && a[2] == 'p' && a[3] == 'o' && a[4] == 's' && a[5] == ';') {
		*c = '\'';
		return 6;
	} else if (a[1] == 'q' && a[2] == 'u' && a[3] == 'o' && a[4] == 't' && a[5] == ';') {
		*c = '"';
		return 6;
	}
	*c = *a;
	return 1;
}

static inline int isname(int c)
{
	return c == '.' || c == '-' || c == '_' || c == ':' ||
		(c >= '0' && c <= '9') ||
		(c >= 'A' && c <= 'Z') ||
		(c >= 'a' && c <= 'z');
}

static inline int iswhite(int c)
{
	return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

static void xml_emit_open_tag(char *a, char *b)
{
	xml_item *head, *tail;
	char *ns;

	/* skip namespace prefix */
	for (ns = a; ns < b; ++ns)
		if (*ns == ':')
			a = ns + 1;

	head = malloc(sizeof *head);
	if (b - a > sizeof(head->name) - 1)
		b = a + sizeof(head->name) - 1;
	memmove(head->name, a, b - a);
	head->name[b - a] = 0;

	head->atts = NULL;
	head->text = NULL;
	head->up = g.head;
	head->down = NULL;
	head->prev = NULL;
	head->next = NULL;

	if (!g.head->down) {
		g.head->down = head;
		g.head->tail = head;
	} else {
		tail = g.head->tail;
		tail->next = head;
		head->prev = tail;
		g.head->tail = head;
	}

	g.head = head;
	g.depth++;
}

static void xml_emit_att_name(char *a, char *b)
{
	xml_item *head = g.head;
	struct attribute *att;

	att = malloc(sizeof *att);
	if (b - a > sizeof(att->name) - 1)
		b = a + sizeof(att->name) - 1;
	memmove(att->name, a, b - a);
	att->name[b - a] = 0;
	att->value = NULL;
	att->next = head->atts;
	head->atts = att;
}

static void xml_emit_att_value(char *a, char *b)
{
	xml_item *head = g.head;
	struct attribute *att = head->atts;
	char *s;
	int c;

	/* entities are all longer than UTFmax so runetochar is safe */
	s = att->value = malloc(b - a + 1);
	while (a < b) {
		if (*a == '&') {
			a += xml_parse_entity(&c, a);
			s += runetochar(s, c);
		} else {
			*s++ = *a++;
		}
	}
	*s = 0;
}

static void xml_emit_close_tag(void)
{
	g.depth--;
	if (g.head->up)
		g.head = g.head->up;
}

static void xml_emit_text(char *a, char *b)
{
	static char *empty = "";
	xml_item *head;
	char *s;
	int c;

	/* Skip text outside the root tag */
	if (g.depth == 0)
		return;

	/* Skip all-whitespace text nodes */
	if (!g.preserve_white) {
		for (s = a; s < b; s++)
			if (!iswhite(*s))
				break;
		if (s == b)
			return;
	}

	xml_emit_open_tag(empty, empty);
	head = g.head;

	/* entities are all longer than UTFmax so runetochar is safe */
	s = head->text = malloc(b - a + 1);
	while (a < b) {
		if (*a == '&') {
			a += xml_parse_entity(&c, a);
			s += runetochar(s, c);
		} else {
			*s++ = *a++;
		}
	}
	*s = 0;

	xml_emit_close_tag();
}

static void xml_emit_cdata(char *a, char *b)
{
	static char *empty = "";
	xml_item *head;
	char *s;

	xml_emit_open_tag(empty, empty);
	head = g.head;

	s = head->text = malloc(b - a + 1);
	while (a < b)
		*s++ = *a++;
	*s = 0;

	xml_emit_close_tag();
}

static char *xml_parse_imp(char *p)
{
	char *mark;
	int quote;

parse_text:
	mark = p;
	while (*p && *p != '<') ++p;
	if (mark != p) xml_emit_text(mark, p);
	if (*p == '<') { ++p; goto parse_element; }
	return NULL;

parse_element:
	if (*p == '/') { ++p; goto parse_closing_element; }
	if (*p == '!') { ++p; goto parse_comment; }
	if (*p == '?') { ++p; goto parse_processing_instruction; }
	while (iswhite(*p)) ++p;
	if (isname(*p))
		goto parse_element_name;
	return "syntax error in element";

parse_comment:
	if (*p == '[') goto parse_cdata;
	if (*p == 'D' && !memcmp(p, "DOCTYPE", 7)) goto parse_declaration;
	if (*p == 'E' && !memcmp(p, "ENTITY", 6)) goto parse_declaration;
	if (*p++ != '-') return "syntax error in comment (<! not followed by --)";
	if (*p++ != '-') return "syntax error in comment (<!- not followed by -)";
	while (*p) {
		if (p[0] == '-' && p[1] == '-' && p[2] == '>') {
			p += 3;
			goto parse_text;
		}
		++p;
	}
	return "end of data in comment";

parse_declaration:
	while (*p) if (*p++ == '>') goto parse_text;
	return "end of data in declaration";

parse_cdata:
	if (p[1] != 'C' || p[2] != 'D' || p[3] != 'A' || p[4] != 'T' || p[5] != 'A' || p[6] != '[')
		return "syntax error in CDATA section";
	p += 7;
	mark = p;
	while (*p) {
		if (p[0] == ']' && p[1] == ']' && p[2] == '>') {
			xml_emit_cdata(mark, p);
			p += 3;
			goto parse_text;
		}
		++p;
	}
	return "end of data in CDATA section";

parse_processing_instruction:
	while (*p) {
		if (p[0] == '?' && p[1] == '>') {
			p += 2;
			goto parse_text;
		}
		++p;
	}
	return "end of data in processing instruction";

parse_closing_element:
	while (iswhite(*p)) ++p;
	while (isname(*p)) ++p;
	while (iswhite(*p)) ++p;
	if (*p != '>')
		return "syntax error in closing element";
	xml_emit_close_tag();
	++p;
	goto parse_text;

parse_element_name:
	mark = p;
	while (isname(*p)) ++p;
	xml_emit_open_tag(mark, p);
	if (*p == '>') { ++p; goto parse_text; }
	if (p[0] == '/' && p[1] == '>') {
		xml_emit_close_tag();
		p += 2;
		goto parse_text;
	}
	if (iswhite(*p))
		goto parse_attributes;
	return "syntax error after element name";

parse_attributes:
	while (iswhite(*p)) ++p;
	if (isname(*p))
		goto parse_attribute_name;
	if (*p == '>') { ++p; goto parse_text; }
	if (p[0] == '/' && p[1] == '>') {
		xml_emit_close_tag();
		p += 2;
		goto parse_text;
	}
	return "syntax error in attributes";

parse_attribute_name:
	mark = p;
	while (isname(*p)) ++p;
	xml_emit_att_name(mark, p);
	while (iswhite(*p)) ++p;
	if (*p == '=') { ++p; goto parse_attribute_value; }
	return "syntax error after attribute name";

parse_attribute_value:
	while (iswhite(*p)) ++p;
	quote = *p++;
	if (quote != '"' && quote != '\'')
		return "missing quote character";
	mark = p;
	while (*p && *p != quote) ++p;
	if (*p == quote) {
		xml_emit_att_value(mark, p++);
		goto parse_attributes;
	}
	return "end of data in attribute value";
}

xml_item *
xml_parse(char *s, int preserve_white, char **errorp)
{
	xml_item root, *node;
	char *error;

	memset(&root, 0, sizeof root);
	g.head = &root;
	g.preserve_white = preserve_white;
	g.depth = 0;

	error = xml_parse_imp(s);
	if (error) {
		if (errorp)
			*errorp = error;
		xml_free(root.down);
		return NULL;
	}

	for (node = root.down; node; node = node->next)
		node->up = NULL;

	if (errorp)
		*errorp = NULL;
	return root.down;
}

#ifdef TEST
#include <stdio.h>

static void dump(xml_item *item)
{
	while (item) {
		char *tag = xml_tag(item);
		if (tag) {
			printf("<%s>\n", tag);
			dump(xml_down(item));
			printf("</%s>\n", tag);
		} else {
			printf("%s\n", xml_text(item));
		}
		item = xml_next(item);
	}
}

int main(int argc, char **argv)
{
	xml_item *xml;
	char *error, *s;
	FILE *f;
	int n;

	if (argc < 2) {
		fprintf(stderr, "usage: xml filename");
		return 1;
	}
	f = fopen(argv[1], "rb");
	if (!f) {
		fprintf(stderr, "cannot open '%s'\n", argv[1]);
		return 1;
	}
	fseek(f, 0, SEEK_END);
	n = ftell(f);
	s = malloc(n + 1);
	fseek(f, 0, SEEK_SET);
	fread(s, 1, n, f);
	s[n] = 0;
	fclose(f);

	xml = xml_parse(s, 0, &error);
	if (!xml) {
		fprintf(stderr, "xml parse error: %s\n", error);
		return 1;
	}
	dump(xml);
	xml_free(xml);

	return 0;
}
#endif
