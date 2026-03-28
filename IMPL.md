# IMPL — F9S Implementation Details

## Overview

**F9S** (FORTRAN 95 Subset) is a compiler targeting Windows x64. The REPL provides `.load` and `.build` commands to compile `.f9s` source files to native executables. All integer arithmetic is in pure NASM assembly. C handles parsing, code generation, and stdio.

---

## Build System — `Makefile` + sub-Makefiles

```
CC   ?= clang
NASM ?= nasm
```

The root `Makefile` delegates compilation to two sub-Makefiles:

| Sub-Makefile | Directory | Action |
|---|---|---|
| `src/asm/Makefile` | `src/asm/` | NASM `-f win64` → `obj/*.obj` |
| `src/c/Makefile` | `src/c/` | `clang -c -m64 -I../include` → `obj/*.obj` |

The root then links all `obj/*.obj` files with `clang -m64 -o bin/program.exe`.

**Directory layout**
```
src/
  asm/           — .asm sources + Makefile
  c/             — .c sources + Makefile
  include/       — shared .h headers
obj/             — all .obj outputs
bin/             — final executable
```

---

## F9S REPL — `src/asm/main.asm`

**Calling convention**: Windows x64 — first four integer args in `rcx, rdx, r8, r9`. Stack aligned to 16 bytes; 32-byte shadow space reserved.

### Startup sequence

1. `call check_features` — returns feature mask in `eax` (bit 0 = SSE, bit 1 = AVX)
2. Store mask into `feature_flags`
3. Print `[LOG] SSE=yes/no AVX=yes/no`

### REPL loop

```
.loop:
    write_str("F9S> ")
    read_line(namebuf, 128)
    if namebuf[0] == 0 → .loop

    r = handle_load_command(namebuf)
    if r != -1 → .loop  ; handled

    r = handle_build_command(namebuf)
    if r != -1 → .loop  ; handled

    if is_exit(namebuf) → .exit
    r = compute(namebuf, feature_flags, &result)
    ; ... dispatch on error codes
    jmp .loop
```

---

## .load / .build Commands

### `src/c/load.c`

| Function | Description |
|---|---|
| `handle_load_command(line)` | Parse `.load <file>`, compile+run. Returns 0=OK, -1=not a .load, -2=error |
| `handle_build_command(line)` | Parse `.build <file>`, compile only. Returns 0=OK, -1=not a .build, -2=error |
| `load_file(filename, table)` | Compile `.f9s` and execute resulting `.exe` |

### `src/c/build.c`

| Function | Description |
|---|---|
| `compile_f9s(filename, table, exe_path, size)` | Full pipeline: parse → semantic → codegen → nasm → clang |
| `build_file(filename, table)` | Wrapper that calls `compile_f9s` without running |

Pipeline steps:
1. Read `.f9s` source file
2. Parse to AST (`parse_program`)
3. Semantic analysis (`semantic_analysis`)
4. Generate NASM (`generate_code`)
5. Assemble with `nasm -f win64`
6. Link with `clang -m64`

---

## Symbol Table — `src/c/heap.c`

AVL tree implementation for O(log n) symbol lookup.

| Function | Description |
|---|---|
| `symbol_table_create()` | Allocate empty table |
| `symbol_table_destroy()` | Free all nodes |
| `symbol_insert(table, name, type, value)` | Insert or update variable |
| `symbol_lookup(table, name)` | Find variable by name |

Type-safe storage: `int_val`, `real_val`, `string_val`, `complex_val`.

---

## Parser — `src/c/parser.c`

Recursive descent parser for the F9S grammar.

### Lexer tokens

- Keywords: `TOK_KW_PROGRAM`, `TOK_KW_END`, `TOK_KW_PRINT`, `TOK_KW_INTEGER`, `TOK_KW_REAL`, `TOK_KW_LOGICAL`, `TOK_KW_IF`, `TOK_KW_THEN`, `TOK_KW_ELSE`, `TOK_KW_DO`, `TOK_KW_STOP`, `TOK_KW_HASHMAP`
- Literals: `TOK_INT_LIT`, `TOK_REAL_LIT`, `TOK_STRING_LIT`, `TOK_LOGICAL_LIT`, `TOK_COMPLEX_LIT`
- Punctuation: `TOK_DCOLON` (`::`), `TOK_ASSIGN_OP` (`:=`), `TOK_EQUALS` (`=`), `TOK_COMMA`, `TOK_LPAREN`, `TOK_RPAREN`
- Comparison: `TOK_EQ` (`==`), `TOK_NE` (`/=`), `TOK_LT` (`<`), `TOK_GT` (`>`), `TOK_LE` (`<=`), `TOK_GE` (`>=`)

### Grammar (subset)

```
program      → PROGRAM name NEWLINE stmt* END PROGRAM name
stmt         → type_decl | array_decl | hashmap_decl
             | assign_implicit | array_assign | hashmap_insert
             | print_stmt | if_stmt | do_stmt | stop_stmt
type_decl    → type '::' IDENT ':=' expr
array_decl   → type '::' IDENT '(' INT_LIT ')'
hashmap_decl → HASHMAP '::' IDENT
assign_implt → IDENT '=' expr
array_assign → IDENT '(' expr ')' '=' expr
hashmap_ins  → IDENT '(' expr ')' ':=' expr
print_stmt   → PRINT '*' ',' (expr | implied_do)
implied_do   → '(' expr ',' IDENT '=' expr ',' expr [',' expr] ')'
if_stmt      → IF '(' cond ')' THEN NEWLINE stmt* [ELSE NEWLINE stmt*] END IF
do_stmt      → DO IDENT '=' expr ',' expr [',' expr] NEWLINE stmt* END DO
stop_stmt    → STOP
condition    → expr rel_op expr
expr         → term (('+' | '-') term)*
term         → factor (('*' | '/') factor)*
factor       → literal | IDENT | IDENT '(' expr ')' | '(' expr ')'
```

### Implied-DO detection

`looks_like_implied_do()` saves the lexer state, speculatively scans for the `, IDENT =` pattern inside `(...)`, then restores state without consuming any tokens. This avoids grammar ambiguity without backtracking overhead.

---

## Code Generator — `src/c/codegen.c`

Emits NASM x64 targeting the Windows calling convention.

### Type support

| Type | Storage | Print format |
|---|---|---|
| INTEGER | 64-bit signed (`resq 1`) | `%lld` |
| REAL | 64-bit double (SSE2) | `%f` |
| LOGICAL | 64-bit (0/1) | `.TRUE.`/`.FALSE.` |
| STRING | pointer to `.data` label | `%s` |

### Phase 6 additions

#### Arrays

- **BSS**: `arr_<name> resq <size>` (emitted by `cg_emit_bss_vars`)
- **1-based indexing**: codegen subtracts 1 from index, multiplies by 8 for byte offset

Array assign (`arr(i) = expr`):
```nasm
; compute value → push rax
; compute index → dec rax; shl rax, 3
lea rcx, [rel arr_<name>]
pop rbx
mov [rcx + rax], rbx
```

Array ref (`arr(i)` as rvalue):
```nasm
; compute index → dec rax; shl rax, 3
lea rcx, [rel arr_<name>]
mov rax, [rcx + rax]
```

#### Hashmaps

- **BSS**: `hm_<name> resb 2048` (64 slots × 32 bytes)
- **Slot layout**: `key(8), value(8), used(8), padding(8)`
- **Hash function**: `slot = key & 63` with linear probing

Hashmap insert (`map(k) := v`):
```nasm
; compute key → push rax
; compute value → mov r8, rax
pop rdx
lea rcx, [rel hm_<name>]
sub rsp, 32
call _f9s_hm_insert
add rsp, 32
```

Hashmap lookup (`map(k)` as rvalue):
```nasm
; compute key → mov rdx, rax
lea rcx, [rel hm_<name>]
sub rsp, 32
call _f9s_hm_lookup
add rsp, 32
; result in rax
```

The runtime helpers `_f9s_hm_insert` / `_f9s_hm_lookup` are emitted inline into the generated `.asm` file (by `cg_emit_hashmap_runtime`) when any hashmap is declared.

#### Implied DO

```nasm
; initialise loop variable
mov rax, <start>
mov [rel var_<i>], rax
.<top>:
    cmp [rel var_<i>], <end>
    jg .<end>
    ; gen_expr(expr) + emit printf
    ; increment
    mov rax, [rel var_<i>]
    add rax, <step>
    mov [rel var_<i>], rax
    jmp .<top>
.<end>:
```

### Control flow patterns

| Construct | Emitted NASM pattern |
|---|---|
| `IF … THEN … END IF` | `cmp` + conditional jump to end label |
| `IF … THEN … ELSE … END IF` | `cmp` + jump to else; unconditional jump over else |
| `DO i = start, stop [, step]` | init → `cmp` at top; `jg` to exit; body; add/sub + `jmp` back |
| `STOP` | `xor ecx, ecx` + `call ExitProcess` |

### Windows x64 printf quirk

For REAL values, both `xmm1` and `rdx` must be set:
```nasm
movsd xmm1, xmm0      ; for printf's %f
movq  rdx,  xmm0      ; integer mirror for Windows varargs
```

---

## Codegen Helpers — `src/c/codegen_helpers.c`

| Function | Description |
|---|---|
| `cg_emit_data_literals(root, out)` | Scan AST for REAL literals, emit `_rcN dq …` |
| `cg_emit_data_strings(root, out)` | Scan AST for STRING literals, emit `_strN db …` |
| `cg_emit_bss_vars(root, out)` | Walk stmts, emit `var_/arr_/hm_` BSS entries |
| `cg_register_array(name, size, type)` | Register array in internal registry |
| `cg_register_hashmap(name)` | Register hashmap in internal registry |
| `cg_is_array(name)` | Query: is this name an array? |
| `cg_is_hashmap(name)` | Query: is this name a hashmap? |
| `cg_array_elem_type(name)` | Get array element type |
| `cg_emit_hashmap_runtime(out)` | Emit `_f9s_hm_insert` / `_f9s_hm_lookup` |
| `cg_helpers_reset()` | Reset all registries before each compilation |

---

## Error Handling — `src/c/error_handler.c`

| Function | Severity | Effect |
|---|---|---|
| `pat(fmt, ...)` | Warning | Increment `g_pat_count`, print yellow |
| `slap(fmt, ...)` | Error | Increment `g_slap_count`, set flag, print red |
| `slap_occurred()` | Query | Returns 1 if any slaps since last reset |
| `reset_diagnostics()` | Reset | Clear both counters and slap flag |
| `print_pat_summary()` | Report | Print warning count if > 0 |

---

## Type Conversion — `src/c/assignment.c`

`convert_value(dest, dest_type, src, src_type, &pat_count)`:

| From → To | Behavior |
|---|---|
| INT → REAL | Widening (pat) |
| REAL → INT | Truncation (pat) |
| STRING → INT/REAL | Parse at compile time; slap if invalid |
| LOGICAL → INT | `.TRUE.` = 1, `.FALSE.` = 0 (pat) |
| Any → STRING | Always slap |

---

## Data Flow

```
F9S> .load hello.f9s
    │
    ├─ handle_load_command()     → src/c/load.c
    │     ├─ extract_filename()  → parse ".load hello.f9s"
    │     └─ load_file()
    │           ├─ compile_f9s() → src/c/build.c
    │           │     ├─ parse_program()      → parser.c  (AST)
    │           │     ├─ semantic_analysis()  → parser.c  (symbol resolution)
    │           │     ├─ generate_code()      → codegen.c (NASM emission)
    │           │     ├─ system("nasm ...")   → hello.obj
    │           │     └─ system("clang ...")  → hello.exe
    │           └─ system("hello.exe")        → program output
    │
    └─ [SUCCESS] Compiled hello.f9s -> hello.exe
       [LOG] Running hello.exe...
       42
       3.14159
```
