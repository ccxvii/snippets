#include <stdio.h>
#include <setjmp.h>

static jmp_buf stack[32];
static int top = -1;

jmp_buf *push_jmp_buf(void) { return &stack[++top]; }
jmp_buf *pop_jmp_buf(void) { return &stack[top--]; }

void throw(void) { longjmp(*pop_jmp_buf(), 1); }

#define try \
	if (!setjmp(*push_jmp_buf())) { do

#define catch \
	while (0); pop_jmp_buf(); } else

main()
{
	try {
		printf("foo!\n");
		throw();
		printf("fee?\n");
	} catch {
		printf("bar!\n");
	}
}
