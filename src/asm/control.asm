; @file control.asm
; @brief Monotonic label-index allocator for code generation.
;
; label_alloc() returns the next unique integer (0, 1, 2, …) that the
; C code generator uses to construct unique branch labels, e.g. _L0, _L1.
;
; label_reset() resets the counter to 0 and must be called before
; each file is compiled so labels start fresh per translation unit.

; F9S - FORTRAN 95 Subset Compiler
; Copyright (C) 2026 Debaditya Malakar
; SPDX-License-Identifier: AGPL-3.0-only

global label_alloc
global label_reset

section .bss
s_counter resq 1   ; monotonic label-id counter (64-bit, zero-initialised)

section .text

; int label_alloc(void)  — returns current counter value, then increments
label_alloc:
    mov  rax, [rel s_counter]
    inc  qword [rel s_counter]
    ret

; void label_reset(void)  — reset counter to 0
label_reset:
    xor  eax, eax
    mov  [rel s_counter], rax
    ret
