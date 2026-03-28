; @file enable.asm
; @brief CPU feature detection via CPUID.
;
; check_features() returns a bitmask:
;   bit 0 — SSE2 available  (CPUID.1 EDX bit 25)
;   bit 1 — AVX available   (CPUID.1 ECX bits 27+28, XCR0 bits 1+2)
;
; The result is stored in feature_flags (main.asm) and passed to
; compute() so that sqrt_int (SSE) vs sqrt_int_fallback (C) is selected.

; F9S - FORTRAN 95 Subset Compiler
; Copyright (C) 2026 Debaditya Malakar
; SPDX-License-Identifier: AGPL-3.0-only

global check_features

section .text

; int check_features(void)  — returns feature bitmask in eax
check_features:
    push rbx
    xor ebx, ebx

    ; CPUID leaf 1: EDX bit 25 = SSE, ECX bits 27+28 = OSXSAVE+AVX
    mov eax, 1
    cpuid

    test edx, 1 << 25      ; SSE present?
    jz .no_sse
    or ebx, 1              ; set bit 0

    ; AVX requires both OSXSAVE (bit 27) and AVX (bit 28) in ECX
    test ecx, (1 << 27) | (1 << 28)
    jz .finish

    ; Verify OS has saved SSE (bit 1) and AVX (bit 2) state via XCR0
    xor ecx, ecx
    xgetbv
    test eax, 0x6
    jnz .have_avx
    jmp .finish

.have_avx:
    or ebx, 2              ; set bit 1
    jmp .finish

.no_sse:
    xor ebx, ebx

.finish:
    mov eax, ebx
    pop rbx
    ret
