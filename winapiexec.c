#include <windows.h>
#include "buffer.h"

#define DEF_VERSION L"1.0"

extern DWORD_PTR __stdcall ParseExecFunction(WCHAR ***pp_argv);

DWORD_PTR ParseExecArgs(WCHAR ***pp_argv);
DWORD_PTR __stdcall GetFunctionPtr(WCHAR ***pp_argv);
DWORD_PTR __stdcall GetNextArg(WCHAR ***pp_argv, BOOL *pbNoMoreArgs);
__declspec(noreturn) void __stdcall FatalStackError(WCHAR **p_argv);
FARPROC MyGetProcAddress(WCHAR *pszModuleProcStr);
DWORD_PTR ParseArg(WCHAR *pszArg);
DWORD_PTR ParseArrayArg(WCHAR *pszArrayArg);
WCHAR *StrToDwordPtr(WCHAR *pszStr, DWORD_PTR *pdw);
char *UnicodeToAscii(WCHAR *pszUnicode);
__declspec(noreturn) void FatalExitMsgBox(WCHAR *format, ...);

int argc;
WCHAR **argv;

void main()
{
	WCHAR **p_argv;

	argv = CommandLineToArgvW(GetCommandLine(), &argc);
	if(!argv)
	{
		ExitProcess(1);
	}

	if(argc < 2)
	{
		MessageBox(NULL, L"See readme or visit http://rammichael.com/", L"winapiexec v" DEF_VERSION, MB_ICONASTERISK);
		ExitProcess(0);
	}

	p_argv = &argv[1];
	ExitProcess((UINT)ParseExecArgs(&p_argv));
}

DWORD_PTR ParseExecArgs(WCHAR ***pp_argv)
{
	DWORD_PTR dwRet;
	WCHAR *argv;

	dwRet = ParseExecFunction(pp_argv);
	argv = **pp_argv;

	// After calling ParseExecFunction, argv is supposed
	// to point at a NULL pointer, or ",", or ")"

	while(argv && argv[0] == L',' && argv[1] == L'\0')
	{
		(*pp_argv)++;
		argv = **pp_argv;
		if(!argv)
			break;

		dwRet = ParseExecFunction(pp_argv);
		argv = **pp_argv;
	}

	return dwRet;
}

DWORD_PTR __stdcall GetFunctionPtr(WCHAR ***pp_argv)
{
	DWORD_PTR dwRet;
	WCHAR *argv = **pp_argv;

	dwRet = (DWORD_PTR)MyGetProcAddress(argv);
	(*pp_argv)++;

	return dwRet;
}

DWORD_PTR __stdcall GetNextArg(WCHAR ***pp_argv, BOOL *pbNoMoreArgs)
{
	DWORD_PTR dwRet;
	WCHAR *argv = **pp_argv;
	if(!argv)
	{
		*pbNoMoreArgs = TRUE;
		return 0;
	}

	if(argv[0] != L'\0' && argv[1] == L'\0')
	{
		switch(argv[0])
		{
		case L',':
		case L')':
			*pbNoMoreArgs = TRUE;
			return 0;

		case L'(':
			(*pp_argv)++;
			argv = **pp_argv;
			if(!argv)
			{
				*pbNoMoreArgs = TRUE;
				return 0;
			}

			dwRet = ParseExecArgs(pp_argv);
			argv = **pp_argv;

			// After calling ParseExecArgs, argv is supposed
			// to point at a NULL pointer, or ")"

			if(argv && argv[0] == L')' && argv[1] == L'\0')
				(*pp_argv)++;

			*pbNoMoreArgs = FALSE;
			return dwRet;
		}
	}

	dwRet = ParseArg(argv);
	*(DWORD_PTR *)*pp_argv = dwRet;
	(*pp_argv)++;

	*pbNoMoreArgs = FALSE;
	return dwRet;
}

__declspec(noreturn) void __stdcall FatalStackError(WCHAR **p_argv)
{
	int nArgIndex = (int)(p_argv - argv);
	FatalExitMsgBox(L"Stack error on argument number %d", nArgIndex);
}

FARPROC MyGetProcAddress(WCHAR *pszModuleProcStr)
{
	WCHAR *psz;
	WCHAR *pszModule, *pszProc;
	char *pszAsciiProc;
	HMODULE hModule;
	FARPROC fpProc;

	psz = pszModuleProcStr;
	while(*psz != L'\0' && *psz != L'@')
		psz++;

	if(*psz == L'\0')
	{
		pszModule = L"kernel32.dll";
		pszProc = pszModuleProcStr;
	}
	else
	{
		*psz = L'\0';
		pszProc = psz + 1;

		pszModule = pszModuleProcStr;

		if(pszModule[0] == L'\0')
		{
			pszModule = L"kernel32.dll";
		}
		else if(pszModule[1] == L'\0')
		{
			if(pszModule[0] == L'k' || pszModule[0] == L'K')
				pszModule = L"kernel32.dll";
			else if(pszModule[0] == L'u' || pszModule[0] == L'U')
				pszModule = L"user32.dll";
		}
	}

	hModule = GetModuleHandle(pszModule);
	if(!hModule)
	{
		hModule = LoadLibrary(pszModule);
		if(!hModule)
			FatalExitMsgBox(L"Could not load module %s", pszModule);
	}

	pszAsciiProc = UnicodeToAscii(pszProc);
	fpProc = GetProcAddress(hModule, pszAsciiProc);
	HeapFree(GetProcessHeap(), 0, pszAsciiProc);

	if(!fpProc)
		FatalExitMsgBox(L"Could not find procedure %s in module %s", pszProc, pszModule);

	return fpProc;
}

DWORD_PTR ParseArg(WCHAR *pszArg)
{
	BOOL bParsed;
	WCHAR *pszStr;
	char *pszAsciiStr;
	DWORD_PTR dw, dw2;

	bParsed = FALSE;

	if(pszArg[0] == L'$' && pszArg[1] != L'\0' && pszArg[2] == L':')
	{
		switch(pszArg[1])
		{
		case L's': // ascii string
			pszAsciiStr = UnicodeToAscii(pszArg + 3);
			lstrcpyA((char *)pszArg, pszAsciiStr);
			HeapFree(GetProcessHeap(), 0, pszAsciiStr);

			dw = (DWORD_PTR)pszArg;
			bParsed = TRUE;
			break;

		case L'u': // unicode string
			dw = (DWORD_PTR)(pszArg + 3);
			bParsed = TRUE;
			break;

		case L'b': // buffer
			StrToDwordPtr(pszArg + 3, &dw);
			if(dw > 0)
				dw = (DWORD_PTR)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, dw);
			else
				dw = (DWORD_PTR)pszArg;

			bParsed = TRUE;
			break;

		case L'$': // another arg
			pszStr = StrToDwordPtr(pszArg + 3, &dw);
			dw = (DWORD_PTR)argv[dw];

			while(*pszStr == '@')
			{
				pszStr = StrToDwordPtr(pszStr + 1, &dw2);
				dw = ((DWORD_PTR *)dw)[dw2];
			}

			bParsed = TRUE;
			break;

		case L'a': // array
			dw = ParseArrayArg(pszArg);
			bParsed = TRUE;
			break;
		}
	}
	else if(pszArg[0] == L'$' && pszArg[1] == L'a' && pszArg[2] == L'[')
	{
		// array brackets syntax, e.g. "$a[1,2,3]"
		dw = ParseArrayArg(pszArg);
		bParsed = TRUE;
	}

	if(!bParsed)
	{
		pszStr = StrToDwordPtr(pszArg, &dw);
		if(*pszStr == L'\0')
			bParsed = TRUE;
	}

	if(bParsed)
		return dw;

	return (DWORD_PTR)pszArg;
}

DWORD_PTR ParseArrayArg(WCHAR *pszArrayArg)
{
	BOOL bBracketsSyntax;
	int nNestingCount;
	int nArrayCount;
	int i;
	DWORD_PTR *pdw;
	WCHAR *pszArrayItem, *pszNextItem;

	// bBracketsSyntax is TRUE for "$a[1,2,3]", FALSE for "$a:1,2,3"
	bBracketsSyntax = (pszArrayArg[2] == L'[');
	nNestingCount = 0;

	pszArrayArg[2] = ',';
	nArrayCount = 0;

	for(i = 2; pszArrayArg[i] != L'\0'; i++)
	{
		if(bBracketsSyntax && pszArrayArg[i] == L']')
		{
			if(nNestingCount == 0)
			{
				pszArrayArg[i] = L'\0';
				break;
			}

			nNestingCount--;
		}

		if(nNestingCount == 0 && pszArrayArg[i] == L',')
		{
			pszArrayArg[i] = L'\0';
			nArrayCount++;

			if(bBracketsSyntax &&
				pszArrayArg[i + 1] == L'$' &&
				pszArrayArg[i + 2] == L'a' &&
				pszArrayArg[i + 3] == L'[')
			{
				nNestingCount++;
				i += 3;
			}
		}
	}

	pdw = (DWORD_PTR *)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, nArrayCount * sizeof(DWORD_PTR));

	pszArrayItem = pszArrayArg + 3;
	for(i = 0; i < nArrayCount - 1; i++)
	{
		pszNextItem = pszArrayItem + lstrlen(pszArrayItem) + 1;
		pdw[i] = ParseArg(pszArrayItem);
		pszArrayItem = pszNextItem;
	}
	pdw[i] = ParseArg(pszArrayItem);

	return (DWORD_PTR)pdw;
}

WCHAR *StrToDwordPtr(WCHAR *pszStr, DWORD_PTR *pdw)
{
	BOOL bMinus;
	DWORD_PTR dw, dw2;

	if(*pszStr == L'-')
	{
		bMinus = TRUE;
		pszStr++;
	}
	else
		bMinus = FALSE;

	dw = 0;

	if(pszStr[0] == L'0' && (pszStr[1] == L'x' || pszStr[1] == L'X'))
	{
		pszStr += 2;

		while(*pszStr != L'\0')
		{
			if(*pszStr >= L'0' && *pszStr <= L'9')
				dw2 = *pszStr - L'0';
			else if(*pszStr >= L'a' && *pszStr <= L'f')
				dw2 = *pszStr - 'a' + 0x0A;
			else if(*pszStr >= L'A' && *pszStr <= L'F')
				dw2 = *pszStr - 'A' + 0x0A;
			else
				break;

			dw <<= 0x04;
			dw |= dw2;
			pszStr++;
		}
	}
	else
	{
		while(*pszStr != L'\0')
		{
			if(*pszStr >= L'0' && *pszStr <= L'9')
				dw2 = *pszStr - L'0';
			else
				break;

			dw *= 10;
			dw += dw2;
			pszStr++;
		}
	}

	if(bMinus)
		*pdw = (DWORD_PTR)-(LONG_PTR)dw; // :)
	else
		*pdw = dw;

	return pszStr;
}

char *UnicodeToAscii(WCHAR *pszUnicode)
{
	char *pszAscii;
	int size;

	size = WideCharToMultiByte(CP_ACP, 0, pszUnicode, -1, NULL, 0, NULL, NULL);
	pszAscii = (char *)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, size);
	WideCharToMultiByte(CP_ACP, 0, pszUnicode, -1, pszAscii, size, NULL, NULL);

	return pszAscii;
}

__declspec(noreturn) void FatalExitMsgBox(WCHAR *format, ...)
{
	WCHAR buffer[1024 + 1];
	va_list args;

	va_start(args, format);

	wvsprintf(buffer, format, args);
	MessageBox(NULL, buffer, NULL, MB_ICONHAND);
	ExitProcess(0);

	va_end(args);
}
