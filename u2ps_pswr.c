#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include "config.h"
#include "warn.h"

/* The PostScript writer.

   The term part wants to output (utf-8) characters one by one,
   and non-string commands as well, without the need to care about
   ( and )u placement.

   The commands themselves should be spaced, possibly wrapping
   the (output) lines to keep them reasonably short.
   A lot of ansi formatting can result in very very long ps output
   even for a single input line. */

extern FILE* output;

enum psmode { NOTHING = 0, COMMAND, UNISTRING, BADSTRING } state = NOTHING;

static int pscol = 0;

void pswrite(const char* string, int len)
{
	pscol += fwrite(string, 1, len, output);
	if(len && string[len-1] == '\n') pscol = 0;
}

void psputs(const char* string)
{
	pswrite(string, strlen(string));
}

void psmode(enum psmode newstate)
{
	if(newstate == state && newstate != COMMAND)
		return;

	switch(state) {
		case UNISTRING: psputs(")u"); state = COMMAND; break;
		case BADSTRING: psputs(")x"); state = COMMAND; break;
		default: break;
	};

	/* possible pairs here:
	   	NOTHING -> COMMAND
		NOTHING -> *STRING
		COMMAND -> *STRING
		COMMAND -> NOTHING
		COMMAND -> COMMAND */

	if(state && newstate)
		psputs(" ");
	else if(!newstate)
		psputs("\n");
	if(newstate == UNISTRING || newstate == BADSTRING)
		psputs("(");

	state = newstate;
}

void psnl(int nl)
{
	int i;
	psmode(NOTHING);
	for(i = 0; i < nl; i++) psputs("\n");
}

void psline(const char* fmt, ...)
{
	va_list ap;
	psmode(NOTHING);
	va_start(ap, fmt);
	vfprintf(output, fmt, ap);
	va_end(ap);
}

void pscmd(const char* fmt, ...)
{
	int nl = (*fmt == '!'); if(nl) fmt++;
	if(!nl && pscol >= PSLINE) psnl(0);

	va_list ap;
	psmode(COMMAND);
	va_start(ap, fmt);
	pscol += vfprintf(output, fmt, ap);
	va_end(ap);

	if(nl) psnl(0);
}

/* Valid utf-8 is written as is, but invalid input gets converted into ????
   by u2ps code to space the ps part from dealing with garbage. */

void psbad(int len)
{
	static char badstring[] = "??????";
	psmode(BADSTRING);
	pswrite(badstring, len < sizeof(badstring) ? len : sizeof(badstring));
}

void psuni(char* ptr, int len)
{
	psmode(UNISTRING);
	if(*ptr == '(' || *ptr == ')' || *ptr == '\\')
		psputs("\\");
	pswrite(ptr, len);
}

/* This is only used for stuff like title, not for the text. */

void psstr(const char* str)
{
	const char* p = str;
	const char* e;

	psmode(COMMAND);
	psputs("(");
	while((e = strpbrk(p, "()\\"))) {
		pswrite(p, e - p);
		psputs("\\");
		pswrite(e, 1);
		p = e + 1;
	} if(*p)
		psputs(p);
	psputs(")");
	state = NOTHING;
}
