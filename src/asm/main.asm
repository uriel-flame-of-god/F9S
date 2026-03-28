; @file main.asm
; @brief REPL entry point for the F9S interactive shell.
;
; Prints the F9S> prompt, reads one line, and dispatches:
;   .load <file>   → handle_load_command  (compile + run)
;   .build <file>  → handle_build_command (compile only)
;   exit / quit    → terminate
;   <expression>   → compute() integer calculator
;
; Windows x64 ABI: first four integer args in rcx/rdx/r8/r9,
; shadow space = 32 bytes. Stack is aligned to 16 bytes at call sites.

; F9S - FORTRAN 95 Subset Compiler
; Copyright (C) 2026 Debaditya Malakar
; SPDX-License-Identifier: AGPL-3.0-only

global main
extern read_line
extern write_str
extern printf
extern detect_type
extern type_name
extern check_features
extern compute
extern is_exit
extern log_info
extern log_warn
extern log_error
extern log_success
extern handle_load_command
extern handle_build_command

section .data
prompt  db "F9S> ", 0
fmt     db "type: %s", 10, 0
logfmt  db 27, "[37;40m[LOG] SSE=%s AVX=%s", 27, "[0m", 10, 0
resfmt  db "result: %lld", 10, 0
yesstr  db "yes", 0
nostr   db "no", 0
msg_sqrt    db "sqrt requires SSE", 0
msg_div0    db "divide by zero", 0
msg_parse   db "parse error", 0
msg_neg     db "sqrt negative", 0
msg_unknown db "unknown op", 0

section .bss
namebuf       resb 128   ; line input buffer
feature_flags resd 1     ; bitmask: bit0=SSE, bit1=AVX
result_q      resq 1     ; calculator result

section .text
main:
    sub rsp, 40          ; shadow space (32) + align (8)

    ; Query CPU features and display SSE/AVX availability
    call check_features
    mov [rel feature_flags], eax
    lea rcx, [rel logfmt]
    test eax, 1
    lea rdx, [rel nostr]
    lea r9,  [rel yesstr]
    cmovnz rdx, r9       ; SSE present?
    test eax, 2
    lea r8,  [rel nostr]
    lea r9,  [rel yesstr]
    cmovnz r8, r9        ; AVX present?
    call printf
    jmp .loop

.loop:
    ; Print prompt and read a line
    lea rcx, [rel prompt]
    call write_str
    lea rcx, [rel namebuf]
    mov edx, 128
    call read_line
    cmp byte [rel namebuf], 0
    je .loop

    ; Try .load command
    lea rcx, [rel namebuf]
    call handle_load_command
    cmp eax, -1
    jne .load_handled

    ; Try .build command
    lea rcx, [rel namebuf]
    call handle_build_command
    cmp eax, -1
    jne .build_handled

    ; Check for exit
    lea rcx, [rel namebuf]
    call is_exit
    test eax, eax
    jne .exit

    ; Run the integer calculator
    lea rcx, [rel namebuf]
    mov edx, [rel feature_flags]
    lea r8,  [rel result_q]
    call compute
    cmp eax, 0
    je .ok
    cmp eax, -2
    je .warn_sse
    cmp eax, -3
    je .err_div0
    cmp eax, -4
    je .err_parse
    cmp eax, -5
    je .err_neg
    lea rcx, [rel msg_unknown]
    call log_warn
    jmp .loop

.warn_sse:
    lea rcx, [rel msg_sqrt]
    call log_warn
    jmp .loop
.err_div0:
    lea rcx, [rel msg_div0]
    call log_error
    jmp .loop
.err_parse:
    lea rcx, [rel msg_parse]
    call log_error
    jmp .loop
.err_neg:
    lea rcx, [rel msg_neg]
    call log_error
    jmp .loop
.ok:
    lea rcx, [rel resfmt]
    mov rdx, [rel result_q]
    call printf
    jmp .loop

.load_handled:
    jmp .loop
.build_handled:
    jmp .loop

.exit:
    xor eax, eax
    add rsp, 40
    ret
