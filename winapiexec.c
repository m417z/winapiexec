#include <windows.h>
#include "args.h"
#include "buffer.h"

#define DEF_VERSION L"1.0"

DWORD __stdcall ParseExecArgs(WCHAR ***p_argv);
FARPROC __stdcall MyGetProcAddress(WCHAR *pszModuleProcStr);
DWORD __stdcall ParseArg(WCHAR *pszArg);
DWORD ParseArrayArg(WCHAR *pszArrayArg);
WCHAR *StrToDword(WCHAR *pszStr, DWORD *pdw);
char *UnicodeToAscii(WCHAR *pszUnicode);
__declspec(noreturn) void __stdcall FatalExitMsgBox(WCHAR *format, ...);

int argc;
TCHAR **argv;

void main()
{
	WCHAR **p_argv;

	setargv(&argc, &argv);

	if(argc < 2)
	{
		MessageBox(NULL, L"See readme or visit http://rammichael.com/", L"winapiexec v" DEF_VERSION, MB_ICONASTERISK);
		ExitProcess(0);
	}

	p_argv = &argv[1];
	ExitProcess(ParseExecArgs(&p_argv));
}

__declspec(naked) DWORD __stdcall ParseExecArgs(WCHAR ***p_argv)
{
	WCHAR *stack_error_msg;

	__asm
	{

	push ebp // stack
	mov ebp, esp
	push ebx // pointer to the module/proc arg
	push esi // pointer to arg
	push edi // loop again or not (a boolean)

	mov eax, p_argv
	mov esi, dword ptr [eax]

main_loop:
		mov ebx, esi

		// get proc address
		push dword ptr [esi]
		call MyGetProcAddress
		push eax
		add esi, 0x04

		// arguments parse
		xor edi, edi

arguments_parse_loop:
			mov eax, dword ptr [esi]
			test eax, eax
			je arguments_parse_end // no more arguments

			cmp dword ptr [eax], L','
			je arguments_parse_end_comma

			cmp dword ptr [eax], L')'
			je arguments_parse_end_closing_bracket

			cmp dword ptr [eax], L'('
			jnz not_a_nested_call

			add esi, 0x04
			mov eax, dword ptr [esi]
			test eax, eax
			je arguments_parse_end // no more arguments

			mov ecx, p_argv // save to p_argv
			mov dword ptr [ecx], esi
			push ecx
			call ParseExecArgs
			mov ecx, p_argv // load from p_argv
			mov esi, dword ptr [ecx]
			jmp arguments_parse_bottom

not_a_nested_call:
			push eax
			call ParseArg
			mov dword ptr [esi], eax
			add esi, 0x04

arguments_parse_bottom:
			push eax
			jmp arguments_parse_loop

arguments_parse_end_comma:
		inc edi
arguments_parse_end_closing_bracket:
		add esi, 0x04
arguments_parse_end:

		// arguments reverse in stack
		mov eax, esp
		lea ecx, dword ptr [ebp-0x10]

arguments_reverse_loop:
			cmp eax, ecx
			jge arguments_reverse_end
	
			mov edx, dword ptr [eax]
			xchg dword ptr [ecx], edx
			mov dword ptr [eax], edx
	
			add eax, 0x04
			sub ecx, 0x04
			jmp arguments_reverse_loop

arguments_reverse_end:

		// call!
		pop eax
		call eax

		mov dword ptr [ebx], eax

		// check and restore stack
		lea ecx, dword ptr [ebp-0x0C]
		cmp esp, ecx
		jbe stack_is_ok

		sub ebx, argv
		shr ebx, 0x02
		push ebx
		mov ebp, esp // These three lines are knida hack of
		push eax     // pushing a string in inline asm
		} stack_error_msg = L"Stack error on argument number %d"; __asm{
		call FatalExitMsgBox

stack_is_ok:
		mov esp, ecx

		// go again?
		test edi, edi
		je main_end

		cmp dword ptr [esi], 0
		jnz main_loop // stop if no more arguments

main_end:

	// save to p_argv
	mov ecx, p_argv
	mov dword ptr [ecx], esi

	pop edi
	pop esi
	pop ebx
	pop ebp
	ret 0x04

	}
}

FARPROC __stdcall MyGetProcAddress(WCHAR *pszModuleProcStr)
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
		pszProc = psz+1;

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

DWORD __stdcall ParseArg(WCHAR *pszArg)
{
	BOOL bParsed;
	WCHAR *pszStr;
	char *pszAsciiStr;
	DWORD dw, dw2;

	bParsed = FALSE;

	if(pszArg[0] == L'$' && pszArg[1] != L'\0' && pszArg[2] == L':')
	{
		switch(pszArg[1])
		{
		case L's': // ascii string
			pszAsciiStr = UnicodeToAscii(pszArg+3);
			lstrcpyA((char *)pszArg, pszAsciiStr);
			HeapFree(GetProcessHeap(), 0, pszAsciiStr);

			dw = (DWORD)pszArg;
			bParsed = TRUE;
			break;

		case L'u': // unicode string
			dw = (DWORD)(pszArg + 3);
			bParsed = TRUE;
			break;

		case L'b': // buffer
			StrToDword(pszArg+3, &dw);
			if(dw > 0)
				dw = (DWORD)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS | HEAP_ZERO_MEMORY, dw);
			else
				dw = (DWORD)pszArg;

			bParsed = TRUE;
			break;

		case L'$': // another arg
			pszStr = StrToDword(pszArg+3, &dw);
			dw = (DWORD)argv[dw];

			while(*pszStr == '@')
			{
				pszStr = StrToDword(pszStr+1, &dw2);
				dw = ((DWORD *)dw)[dw2];
			}

			bParsed = TRUE;
			break;

		case L'a': // array
			dw = ParseArrayArg(pszArg + 3);
			bParsed = TRUE;
			break;
		}
	}

	if(!bParsed)
	{
		pszStr = StrToDword(pszArg, &dw);
		if(*pszStr == L'\0')
			bParsed = TRUE;
	}

	if(bParsed)
		return dw;

	return (DWORD)pszArg;
}

DWORD ParseArrayArg(WCHAR *pszArrayArg)
{
	int array_count;
	int i;
	DWORD *pdw;
	WCHAR *pszNext;

	array_count = 1;

	for(i=0; pszArrayArg[i] != L'\0'; i++)
	{
		if(pszArrayArg[i] == L',')
		{
			pszArrayArg[i] = L'\0';
			array_count++;
		}
	}

	pdw = (DWORD *)HeapAlloc(GetProcessHeap(), HEAP_GENERATE_EXCEPTIONS, array_count * sizeof(DWORD));

	for(i=0; i<array_count; i++)
	{
		pszNext = pszArrayArg + lstrlen(pszArrayArg) + 1;
		pdw[i] = ParseArg(pszArrayArg);
		pszArrayArg = pszNext;
	}

	return (DWORD)pdw;
}

WCHAR *StrToDword(WCHAR *pszStr, DWORD *pdw)
{
	BOOL bMinus;
	DWORD dw, dw2;

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
		*pdw = (DWORD)-(long)dw; // :)
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

__declspec(noreturn) void __stdcall FatalExitMsgBox(WCHAR *format, ...)
{
	WCHAR buffer[1024+1];
	va_list args;

	va_start(args, format);

	wvsprintf(buffer, format, args);
	MessageBox(NULL, buffer, NULL, MB_ICONHAND);
	ExitProcess(0);

	va_end(args);
}
