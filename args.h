#ifndef _ARGS_H_
#define _ARGS_H_

#include <windows.h>

void setargv(int *argc, TCHAR ***argv);
static void parse_cmdline(TCHAR *cmdstart, TCHAR **argv, TCHAR *args, int *numargs, int *numchars);
void freeargv(TCHAR **argv);

#endif // _ARGS_H_
