#include "server.h"

int
_needsquotes(char *s, int *quotelenp)
{
	int quoted = 0;
	int quotelen = 0;

	if (*s == '\0') {
		quoted = 1;
		quotelen = 2;
	}

	while (*s) {
		int c = *s;
		if (c <= ' ' || c == '\'') {
			if (!quoted) {
				quoted = 1;
				quotelen += 2;
			}
			if (c == '\'')
				quotelen ++;
		}
		quotelen ++;
		s ++;
	}

	*quotelenp = quotelen;

	return quoted;
}

char*
unquotestrdup(char *s)
{
	char *t, *ret;
	int quoting;

	ret = s = strdup(s);	/* return unquoted copy */
	if(ret == nil)
		return ret;
	quoting = 0;
	t = s;	/* s is output string, t is input string */
	while(*t!='\0' && (quoting || (*t!=' ' && *t!='\t'))){
		if(*t != '\''){
			*s++ = *t++;
			continue;
		}
		/* *t is a quote */
		if(!quoting){
			quoting = 1;
			t++;
			continue;
		}
		/* quoting and we're on a quote */
		if(t[1] != '\''){
			/* end of quoted section; absorb closing quote */
			t++;
			quoting = 0;
			continue;
		}
		/* doubled quote; fold one quote into two */
		t++;
		*s++ = *t++;
	}
	if(t != s)
		memmove(s, t, strlen(t)+1);
	return ret;
}

char*
quotestrdup(char *s)
{
	char *t, *u, *ret;
	int quotelen;
	int r;

	if(_needsquotes(s, &quotelen) == 0)
		return strdup(s);
	
	ret = malloc(quotelen+1);
	if(ret == nil)
		return nil;
	u = ret;
	*u++ = '\'';
	for(t=s; *t; t++){
		r = *t;
		if(r == '\'')
			*u++ = r;	/* double the quote */
		*u++ = r;
	}
	*u++ = '\'';
	*u = '\0';
	return ret;
}

