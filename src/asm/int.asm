; F9S - FORTRAN 95 Subset Compiler
; Copyright (C) 2026 Debaditya Malakar
; SPDX-License-Identifier: AGPL-3.0-only

global add_int
global sub_int
global mul_int
global div_int
global square_int
global sqrt_int

section .text
add_int:
    mov rax, rcx
    add rax, rdx
    ret

sub_int:
    mov rax, rcx
    sub rax, rdx
    ret

mul_int:
    mov rax, rcx
    imul rax, rdx
    ret

div_int:
    test rdx, rdx
    je .zero
    mov r8, rdx
    mov rax, rcx
    cqo
    idiv r8
    ret
.zero:
    xor rax, rax
    ret

square_int:
    mov rax, rcx
    imul rax, rcx
    ret

sqrt_int:
    test rcx, rcx
    js .neg
    cvtsi2sd xmm0, rcx
    sqrtsd xmm0, xmm0
    cvttsd2si rax, xmm0
    ret
.neg:
    xor rax, rax
    ret
