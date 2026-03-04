; F9S - FORTRAN 95 Subset Compiler
; Copyright (C) 2026 Debaditya Malakar
; SPDX-License-Identifier: AGPL-3.0-only

global label_alloc
global label_reset

section .bss
s_counter resq 1        ; monotonic label-id counter (64-bit)

section .text

label_alloc:
    mov  rax, [rel s_counter]
    inc  qword [rel s_counter]
    ret

label_reset:
    xor  eax, eax
    mov  [rel s_counter], rax
    ret
