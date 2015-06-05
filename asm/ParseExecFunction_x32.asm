format MS COFF
section '.text' code readable executable

public ParseExecFunction as '_ParseExecFunction@4'
extrn '_GetFunctionPtr@4' as GetFunctionPtr
extrn '_GetNextArg@8' as GetNextArg
extrn '_FatalStackError@4' as FatalStackError

ParseExecFunction:

	push ebp ; Stack
	mov ebp, esp
	push ebx ; Pointer to the function name argument
	push ecx ; Stack variable, used as bNoMoreArgs

	; Save pointer to the function name argument
	mov ecx, dword [ebp+0x08]
	mov ebx, dword [ecx]

	; Push 16 zeros on the stack, for better safety
	mov ecx, 0x10
push_zeroes_loop:
	push 0
	loop push_zeroes_loop

	; Push function pointer and arguments
	push dword [ebp+0x08]
	call GetFunctionPtr

arguments_parse_loop:
	push eax

	lea ecx, dword [ebp-0x08]
	push ecx
	push dword [ebp+0x08]
	call GetNextArg

	cmp dword [ebp-0x08], 0
	je arguments_parse_loop ; jmp if !bNoMoreArgs

	; Reverse arguments in stack
	mov eax, esp
	lea ecx, dword [ebp-0x4C]

arguments_reverse_loop:
	mov edx, dword [eax]
	xchg dword [ecx], edx
	mov dword [eax], edx
	
	add eax, 0x04
	sub ecx, 0x04

	cmp eax, ecx
	jb arguments_reverse_loop
	
	; Call!
	pop eax
	call eax

	mov dword [ebx], eax

	; Check and restore stack
	lea ecx, dword [ebp-0x48]
	cmp esp, ecx
	jbe stack_is_ok

	push ebx
	call FatalStackError

stack_is_ok:
	lea esp, dword [ebp-0x04]

	; Done
	pop ebx
	pop ebp
	ret 0x04
