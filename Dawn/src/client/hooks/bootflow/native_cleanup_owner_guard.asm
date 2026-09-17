; Reproduce F9C150's native frame exactly. Both jumps enter the original body;
; its original epilogue releases that frame and returns to our C++ call gate.
; A missing owner disables only sibling reassignment, not component destruction.
EXTERN cleanup_owner_cookie:QWORD
EXTERN cleanup_owner_resolver:QWORD
EXTERN cleanup_owner_resume:QWORD
EXTERN cleanup_owner_tail:QWORD
EXTERN cleanup_owner_misses:QWORD
PUBLIC native_cleanup_owner_body
.code
native_cleanup_owner_body PROC FRAME
    mov [rsp+10h], rbx
    mov [rsp+18h], rsi
    push rbp
    .pushreg rbp
    push rdi
    .pushreg rdi
    push r12
    .pushreg r12
    push r14
    .pushreg r14
    push r15
    .pushreg r15
    mov rbp, rsp
    sub rsp, 70h
    .allocstack 70h
    .setframe rbp, 70h
    .savereg rbx, 0A8h
    .savereg rsi, 0B0h
    .endprolog
    mov rax, cleanup_owner_cookie
    mov rax, [rax]
    xor rax, rsp
    mov [rbp-10h], rax
    mov r15, rcx
    call cleanup_owner_resolver
    test rax, rax
    jz missing_owner
    xor edi, edi
    lea rcx, [rbp-40h]
    xor edx, edx
    mov ebx, edi
    mov rsi, [rax+10EB0h]
    ; A RIP-indirect jump is decoded as an epilogue by RtlVirtualUnwind even
    ; though this borrowed frame is still live. Register-indirect transfer is
    ; not an epilogue. R11 is volatile and dead at both native continuations.
    mov r11, cleanup_owner_resume
    jmp r11
missing_owner:
    lock inc cleanup_owner_misses
    xor edi, edi
    lea rcx, [rbp-40h]
    xor edx, edx
    mov ebx, edi
    mov r11, cleanup_owner_tail
    jmp r11
native_cleanup_owner_body ENDP
END
