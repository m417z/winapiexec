format MS64 COFF
section '.text' code readable executable

public ParseExecFunction as 'ParseExecFunction'
extrn 'GetFunctionPtr' as GetFunctionPtr
extrn 'GetNextArg' as GetNextArg
; Note: We don't need FatalStackError as we can't detect stack failures.
;       In x64, caller cleans up the stack arguments.
; extrn 'FatalStackError' as FatalStackError

ParseExecFunction:

	push rbp ; Stack
	mov rbp, rsp
	push rbx ; Pointer to the function name argument
	sub rsp, 0x08 ; for 16 bytes alignment

	; Save arguments
	mov qword [rbp+0x10], rcx ; pp_argv

	; Save pointer to the function name argument
	mov rbx, qword [rcx]

	; Push 16 zeros on the stack, for better safety
	mov rcx, 0x10
push_zeroes_loop:
	push 0
	loop push_zeroes_loop

	; Push function pointer and arguments
	sub rsp, 0x20
	mov rcx, qword [rbp+0x10] ; arg 1: pp_argv
	call GetFunctionPtr
	add rsp, 0x20

arguments_parse_loop:
	push rax

	mov rcx, qword [rbp+0x10] ; arg 1: pp_argv
	lea rdx, dword [rbp+0x18] ; arg 2: uses "scratch space"
	push rbp
	mov rbp, rsp
	sub rsp, 0x20
	and rsp, 0xFFFFFFFFFFFFFFF0 ; 16 bytes alignment
	call GetNextArg
	mov rsp, rbp
	pop rbp

	cmp dword [rbp+0x18], 0
	je arguments_parse_loop ; jmp if !bNoMoreArgs

	; 16 bytes mis-alignment (will be aligned when we pop the function pointer)
	test rsp, 0x08
	jnz @f
	push 0
@@:

	; Reverse arguments in stack
	mov rax, rsp
	lea rcx, qword [rbp-0x98]

arguments_reverse_loop:
	mov rdx, qword [rax]
	xchg qword [rcx], rdx
	mov qword [rax], rdx
	
	add rax, 0x08
	sub rcx, 0x08

	cmp rax, rcx
	jb arguments_reverse_loop
	
	; Call!
	pop rax
	mov rcx, qword [rsp]
	mov rdx, qword [rsp+0x08]
	mov r8, qword [rsp+0x10]
	mov r9, qword [rsp+0x18]
	call rax

	mov qword [rbx], rax

	; Restore stack
	lea rsp, qword [rbp-0x08]

	; Done
	pop rbx
	pop rbp
	ret
