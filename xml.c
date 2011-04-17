#include <stdio.h>
#include <stdlib.h>

struct parser { int foo; };

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

static char *xml_emit_entity(char *a)
{
	int c;
	char *b;
	if (a[1] == '#') {
		if (a[2] == 'x')
			c = strtol(a + 3, &b, 16);
		else
			c = strtol(a + 2, &b, 10);
		if (*b == ';') {
			putchar(c);
			return b + 1;
		}
	} else if (a[1] == 'l' && a[2] == 't' && a[3] == ';') {
		putchar('<');
		return a + 4;
	} else if (a[1] == 'g' && a[2] == 't' && a[3] == ';') {
		putchar('>');
		return a + 4;
	} else if (a[1] == 'a' && a[2] == 'm' && a[3] == 'p' && a[4] == ';') {
		putchar('&');
		return a + 5;
	} else if (a[1] == 'a' && a[2] == 'p' && a[3] == 'o' && a[4] == 's' && a[5] == ';') {
		putchar('\'');
		return a + 6;
	} else if (a[1] == 'q' && a[2] == 'u' && a[3] == 'o' && a[4] == 't' && a[5] == ';') {
		putchar('"');
		return a + 6;
	}
	putchar(*a++);
	return a;
}

static void xml_emit_text(struct parser *x, char *a, char *b)
{
	while (a < b)
		if (*a == '&')
			a = xml_emit_entity(a);
		else
			putchar(*a++);
}

static void xml_emit_cdata(struct parser *x, char *a, char *b)
{
	while (a < b)
		putchar(*a++);
}

static void xml_emit_open_tag(struct parser *x, char *a, char *b)
{
	putchar('<');
	while (a < b) putchar(*a++);
}

static void xml_emit_att_name(struct parser *x, char *a, char *b)
{
	putchar(' ');
	while (a < b) putchar(*a++);
	putchar('=');
}

static void xml_emit_att_value(struct parser *x, char *a, char *b)
{
	putchar('"');
	while (a < b)
		if (*a == '&')
			a = xml_emit_entity(a);
		else
			putchar(*a++);
	putchar('"');
}

static void xml_emit_att_end(struct parser *x)
{
	putchar('>');
}

static void xml_emit_close_tag(struct parser *x)
{
	putchar('<');
	putchar('/');
	putchar('>');
}

char *
xml_parse_document(struct parser *x, char *p)
{
	char *mark;
	int quote;

parse_text:
	mark = p;
	while (*p && *p != '<') ++p;
	if (p > mark) xml_emit_text(x, mark, p);
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
	if (*p++ != '-') return "syntax error in comment (<! not followed by --)";
	if (*p++ != '-') return "syntax error in comment (<!- not followed by -)";
	mark = p;
	while (*p) {
		if (p[0] == '-' && p[1] == '-' && p[2] == '>') {
			p += 3;
			goto parse_text;
		}
		++p;
	}
	return "end of data in comment";

parse_cdata:
	if (p[1] != 'C' || p[2] != 'D' || p[3] != 'A' || p[4] != 'T' || p[5] != 'A' || p[6] != '[')
		return "syntax error in CDATA section";
	p += 7;
	mark = p;
	while (*p) {
		if (p[0] == ']' && p[1] == ']' && p[2] == '>') {
			xml_emit_cdata(x, mark, p);
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
	mark = p;
	while (isname(*p)) ++p;
	while (iswhite(*p)) ++p;
	if (*p != '>')
		return "syntax error in closing element";
	xml_emit_close_tag(x);
	++p;
	goto parse_text;

parse_element_name:
	mark = p;
	while (isname(*p)) ++p;
	xml_emit_open_tag(x, mark, p);
	if (*p == '>') {
		xml_emit_att_end(x);
		++p;
		goto parse_text;
	}
	if (p[0] == '/' && p[1] == '>') {
		xml_emit_close_tag(x);
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
	if (*p == '>') {
		xml_emit_att_end(x);
		++p;
		goto parse_text;
	}
	if (p[0] == '/' && p[1] == '>') {
		xml_emit_att_end(x);
		xml_emit_close_tag(x);
		p += 2;
		goto parse_text;
	}
	return "syntax error in attributes";

parse_attribute_name:
	mark = p;
	while (isname(*p)) ++p;
	xml_emit_att_name(x, mark, p);
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
		xml_emit_att_value(x, mark, p++);
		goto parse_attributes;
	}
	return "end of data in attribute value";
}

void die(char *msg)
{
	fprintf(stderr, "error: %s\n", msg);
	exit(1);
}

int
main(int argc, char **argv)
{
	FILE *file;
	char *buf;
	char *err;
	int i, n;

	for (i = 1; i < argc; i++)
	{
		file = fopen(argv[1], "r");
		if (!file)
			die("cannot open file");
		fseek(file, 0, 2);
		n = ftell(file);
		fseek(file, 0, 0);
		buf = malloc(n + 1);
		n = fread(buf, 1, n, file);
		buf[n] = 0;
		fclose(file);

		err = xml_parse_document(NULL, buf);
		if (err)
			die(err);
		free(buf);
	}

	return 0;
}

