/***
*stdargv.c - standard & wildcard _setargv routine
*
*       Copyright (c) Microsoft Corporation. All rights reserved.
*
*Purpose:
*       processes program command line, with or without wildcard expansion
*
*MODIFIED:
*       removed wildcard support, simplified a couple of stuff
*
*******************************************************************************/

#include "args.h"

#define NULCHAR    TEXT('\0')
#define SPACECHAR  TEXT(' ')
#define TABCHAR    TEXT('\t')
#define DQUOTECHAR TEXT('\"')
#define SLASHCHAR  TEXT('\\')

/***
*_setargv, __setargv - set up "argc" and "argv" for C programs
*
*Purpose:
*       Read the command line and create the argv array for C
*       programs.
*
*Entry:
*       Arguments are retrieved from the program command line,
*       pointed to by _acmdln.
*
*Exit:
*       "argv" points to a null-terminated list of pointers to ASCIZ
*       strings, each of which is an argument from the command line.
*       "argc" is the number of arguments.  The strings are copied from
*       the environment segment into space allocated on the heap/stack.
*       The list of pointers is also located on the heap or stack.
*       _pgmptr points to the program name.
*
*Exceptions:
*       Terminates with out of memory error if no memory to allocate.
*
*******************************************************************************/

void setargv(int *argc, TCHAR ***argv)
{
	TCHAR *p;
	TCHAR *cmdstart;                  /* start of command line to parse */
	int numargs, numchars;

	TCHAR _pgmname[MAX_PATH];

	/* if there's no command line at all (won't happen from cmd.exe, but
	possibly another program), then we use _pgmptr as the command line
	to parse, so that argv[0] is initialized to the program name */

	cmdstart = GetCommandLine();
	if(*cmdstart == '\0')
	{
		/* Get the program name pointer from Win32 Base */
		GetModuleFileName(NULL, _pgmname, MAX_PATH);

		cmdstart = _pgmname;
	}

	/* first find out how much space is needed to store args */
	parse_cmdline(cmdstart, NULL, NULL, &numargs, &numchars);

	/* allocate space for argv[] vector and strings */
	p = (TCHAR *)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, numargs * sizeof(TCHAR *) + numchars * sizeof(TCHAR));

	/* store args and argv ptrs in just allocated block */
	parse_cmdline(cmdstart, (TCHAR **)p, (TCHAR *)((char *)p + numargs * sizeof(TCHAR *)), &numargs, &numchars);

	/* set argv and argc */
	*argc = numargs - 1;
	*argv = (TCHAR **)p;
}

/***
*static void parse_cmdline(cmdstart, argv, args, numargs, numchars)
*
*Purpose:
*       Parses the command line and sets up the argv[] array.
*       On entry, cmdstart should point to the command line,
*       argv should point to memory for the argv array, args
*       points to memory to place the text of the arguments.
*       If these are NULL, then no storing (only counting)
*       is done.  On exit, *numargs has the number of
*       arguments (plus one for a final NULL argument),
*       and *numchars has the number of bytes used in the buffer
*       pointed to by args.
*
*Entry:
*       _TSCHAR *cmdstart - pointer to command line of the form
*           <progname><nul><args><nul>
*       _TSCHAR **argv - where to build argv array; NULL means don't
*                       build array
*       _TSCHAR *args - where to place argument text; NULL means don't
*                       store text
*
*Exit:
*       no return value
*       int *numargs - returns number of argv entries created
*       int *numchars - number of characters used in args buffer
*
*Exceptions:
*
*******************************************************************************/

static void parse_cmdline(TCHAR *cmdstart, TCHAR **argv, TCHAR *args, int *numargs, int *numchars)
{
	TCHAR *p;
	TCHAR c;
	int inquote;                    /* 1 = inside quotes */
	int copychar;                   /* 1 = copy char to *args */
	unsigned numslash;              /* num of backslashes seen */

	*numchars = 0;
	*numargs = 1;                   /* the program name at least */

	/* first scan the program name, copy it, and count the bytes */
	p = cmdstart;
	if(argv)
		*argv++ = args;

	/* A quoted program name is handled here. The handling is much
	simpler than for other arguments. Basically, whatever lies
	between the leading double-quote and next one, or a terminal null
	character is simply accepted. Fancier handling is not required
	because the program name must be a legal NTFS/HPFS file name.
	Note that the double-quote characters are not copied, nor do they
	contribute to numchars. */
	inquote = FALSE;
	do {
		if (*p == DQUOTECHAR )
		{
			inquote = !inquote;
			c = *p++;
			continue;
		}

		++*numchars;
		if (args)
			*args++ = *p;

		c = *p++;
	} while ( (c != NULCHAR && (inquote || (c !=SPACECHAR && c != TABCHAR))) );

	if ( c == NULCHAR ) {
		p--;
	} else {
		if (args)
			*(args-1) = NULCHAR;
	}

	inquote = 0;

	/* loop on each argument */
	for(;;) {

		if ( *p ) {
			while (*p == SPACECHAR || *p == TABCHAR)
				++p;
		}

		if (*p == NULCHAR)
			break;              /* end of args */

		/* scan an argument */
		if (argv)
			*argv++ = args;     /* store ptr to arg */
		++*numargs;

		/* loop through scanning one argument */
		for (;;) {
			copychar = 1;
			/* Rules: 2N backslashes + " ==> N backslashes and begin/end quote
			2N+1 backslashes + " ==> N backslashes + literal "
			N backslashes ==> N backslashes */
			numslash = 0;
			while (*p == SLASHCHAR) {
				/* count number of backslashes for use below */
				++p;
				++numslash;
			}
			if (*p == DQUOTECHAR) {
				/* if 2N backslashes before, start/end quote, otherwise
				copy literally */
				if (numslash % 2 == 0) {
					if (inquote && p[1] == DQUOTECHAR) {
						p++;    /* Double quote inside quoted string */
					} else {    /* skip first quote char and copy second */
						copychar = 0;       /* don't copy quote */
						inquote = !inquote;
					}
				}
				numslash /= 2;          /* divide numslash by two */
			}

			/* copy slashes */
			while (numslash--) {
				if (args)
					*args++ = SLASHCHAR;
				++*numchars;
			}

			/* if at end of arg, break loop */
			if (*p == NULCHAR || (!inquote && (*p == SPACECHAR || *p == TABCHAR)))
				break;

			/* copy character into argument */
			if (copychar) {
				if (args)
					*args++ = *p;
				++*numchars;
			}
			++p;
		}

		/* null-terminate the argument */

		if (args)
			*args++ = NULCHAR;          /* terminate string */
		++*numchars;
	}

	/* We put one last argument in -- a null ptr */
	if (argv)
		*argv++ = NULL;
	++*numargs;
}

void freeargv(TCHAR **argv)
{
	HeapFree(GetProcessHeap(), 0, argv);
}
