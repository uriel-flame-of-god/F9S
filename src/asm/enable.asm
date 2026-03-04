; F9S - FORTRAN 95 Subset Compiler
; Copyright (C) 2026 Debaditya Malakar
; SPDX-License-Identifier: AGPL-3.0-only

global check_features

section .text
check_features:
    push rbx
    xor ebx, ebx
    xor eax, eax
    mov eax, 1
    cpuid
    ; SSE present bit 25 EDX
    test edx, 1 << 25
    jz .no_sse
    or ebx, 1             ; use ebx temp for mask
.have_sse:
    ; check AVX: CPUID.1 ECX bits 27 (OSXSAVE) and 28 (AVX)
    test ecx, (1 << 27) | (1 << 28)
    jz .finish
    ; verify XCR0 bits 1 (SSE state) and 2 (AVX state)
    xor ecx, ecx
    xgetbv
    test eax, 0x6
    jnz .have_avx
    jmp .finish
.have_avx:
    or ebx, 2
    jmp .finish
.no_sse:
    xor ebx, ebx
.finish:
    mov eax, ebx
    pop rbx
    ret
