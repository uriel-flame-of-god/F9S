; @file int.asm
; @brief NASM x64 integer arithmetic primitives.
;
; Windows x64 ABI: integer args in rcx, rdx; return value in rax.
; All functions follow the C calling convention and preserve rbx/rbp/rsi/rdi.
;
; sqrt_int uses SSE2 (sqrtsd) which is baseline on all x64 processors.
; The C fallback sqrt_int_fallback in int_fallback.c handles the rare case
; where SSE2 is unavailable (check_features bit 0 = 0).

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

; long long add_int(long long a, long long b)
add_int:
    mov rax, rcx
    add rax, rdx
    ret

; long long sub_int(long long a, long long b)
sub_int:
    mov rax, rcx
    sub rax, rdx
    ret

; long long mul_int(long long a, long long b)
mul_int:
    mov rax, rcx
    imul rax, rdx
    ret

; long long div_int(long long a, long long b)  — returns 0 for b==0
div_int:
    test rdx, rdx
    je .zero
    mov r8,  rdx
    mov rax, rcx
    cqo
    idiv r8
    ret
.zero:
    xor rax, rax
    ret

; long long square_int(long long a)
square_int:
    mov rax, rcx
    imul rax, rcx
    ret

; long long sqrt_int(long long a)  — floor(sqrt(a)), 0 for negative input
sqrt_int:
    test rcx, rcx
    js .neg
    cvtsi2sd xmm0, rcx    ; int64 → double
    sqrtsd   xmm0, xmm0   ; hardware sqrt
    cvttsd2si rax, xmm0   ; double → int64 (truncate)
    ret
.neg:
    xor rax, rax
    ret
