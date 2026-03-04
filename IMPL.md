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
    
    ; Check for .load command
    r = handle_load_command(namebuf)
    if r != -1 → .loop  ; handled
    
    ; Check for .build command  
    r = handle_build_command(namebuf)
    if r != -1 → .loop  ; handled
    
    ; Calculator fallback
    if is_exit(namebuf) → .exit
    r = compute(namebuf, feature_flags, &result)
    ; ... dispatch on error codes
    jmp .loop
```

---

## .load / .build Commands

### `src/c/load.c`

Command parsing and execution handled entirely in C:

| Function | Description |
|---|---|
| `handle_load_command(line)` | Parse `.load <file>`, compile+run. Returns 0=OK, -1=not a .load, -2=error |
| `handle_build_command(line)` | Parse `.build <file>`, compile only. Returns 0=OK, -1=not a .build, -2=error |
| `load_file(filename, table)` | Compile `.f9s` and execute resulting `.exe` |

### `src/c/build.c`

Core compilation pipeline:

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
| `symbol_insert(table, name, type)` | Insert/update variable |
| `symbol_lookup(table, name)` | Find variable by name |
| `symbol_set_value(sym, val, type)` | Set typed value |

Type-safe storage: `int_val`, `real_val`, `string_val`, `logical_val`, `complex_val`.

---

## Parser — `src/c/parser.c`

Recursive descent parser for F9S grammar.

### Lexer tokens
- `TOK_PROGRAM`, `TOK_END`, `TOK_PRINT`, `TOK_INTEGER`, `TOK_REAL`, `TOK_LOGICAL`
- `TOK_IF`, `TOK_THEN`, `TOK_ELSE`, `TOK_DO`, `TOK_STOP`
- `TOK_IDENTIFIER`, `TOK_INT_LIT`, `TOK_REAL_LIT`, `TOK_STRING_LIT`, `TOK_LOGICAL_LIT`
- Arithmetic: `TOK_PLUS`, `TOK_MINUS`, `TOK_STAR`, `TOK_SLASH`, `TOK_EQUALS`
- Comparison: `TOK_EQ` (`==`), `TOK_NE` (`/=`), `TOK_LT` (`<`), `TOK_GT` (`>`), `TOK_LE` (`<=`), `TOK_GE` (`>=`)
- `TOK_DCOLON` (`::`), `TOK_COMMA`, `TOK_LPAREN`, `TOK_RPAREN`

### Grammar (subset)
```
program     → PROGRAM name NEWLINE stmt* END PROGRAM name
stmt        → declaration | assignment | print_stmt
            | if_stmt | do_stmt | stop_stmt
declaration → type_spec DCOLON var_list
assignment  → IDENTIFIER EQUALS expr
print_stmt  → PRINT STAR COMMA expr
if_stmt     → IF LPAREN condition RPAREN THEN NEWLINE
                  stmt*
              [ ELSE NEWLINE stmt* ]
              END IF
do_stmt     → DO IDENTIFIER EQUALS expr COMMA expr [ COMMA expr ] NEWLINE
                  stmt*
              END DO
stop_stmt   → STOP
condition   → expr rel_op expr
rel_op      → == | /= | < | > | <= | >=
expr        → term ((PLUS|MINUS) term)*
term        → factor ((STAR|SLASH) factor)*
factor      → literal | IDENTIFIER | LPAREN expr RPAREN
```

---

## Code Generator — `src/c/codegen.c`

Emits NASM x64 targeting Windows calling convention.

### Type support
| Type | Storage | Print format |
|---|---|---|
| INTEGER | 64-bit signed | `%lld` |
| REAL | 64-bit double (SSE) | `%f` |
| LOGICAL | 64-bit (0/1) | `.TRUE.`/`.FALSE.` |
| STRING | pointer | `%s` |

### Control flow
| Construct | Emitted NASM pattern |
|---|---|
| `IF … THEN … END IF` | `cmp` + conditional jump to end label |
| `IF … THEN … ELSE … END IF` | `cmp` + jump to else label; unconditional jump over else |
| `DO i = start, stop [, step]` | init → `cmp` at top; `jg` to exit; body; `add`/`sub` + `jmp` back |
| `STOP` | `xor ecx, ecx` + `call ExitProcess` |

### Key functions
- `generate_code(ast, filename)` — main entry point
- `gen_stmt(node)` — dispatch by statement type
- `gen_expr(node)` — recursive expression evaluation
- `gen_condition(node)` — emit `cmp` and return the appropriate `jcc` mnemonic
- `infer_expr_type(node)` — determine result type for proper handling

### Windows x64 printf quirk
For REAL values, must set **both** `xmm1` and `rdx`:
```nasm
movsd xmm1, xmm0      ; for printf's %f
movq rdx, xmm0        ; for Windows varargs
```

### Label naming
Labels are emitted as `.Lif<N>_else`, `.Lif<N>_end`, `.Ldo<N>_top`, `.Ldo<N>_end`
where `N` is a per-function counter incremented for each construct.

---

## Error Handling — `src/c/error_handler.c`

**Pat/Slap** diagnostic system:

| Function | Severity | Effect |
|---|---|---|
| `pat(fmt, ...)` | Warning | Increment `g_pat_count`, print yellow |
| `slap(fmt, ...)` | Error | Increment `g_slap_count`, print red |
| `slap_occurred()` | Query | Returns true if any slaps |
| `reset_diagnostics()` | Reset | Clear both counters |
| `print_pat_summary()` | Report | Print "N pats, M slaps" if any |

---

## Type Conversion — `src/c/assignment.c`

`convert_value(src, src_type, dest_type)` — converts between types with pat/slap policy:

| From → To | Behavior |
|---|---|
| INT → REAL | Lossless |
| REAL → INT | Truncates, **pat** if precision lost |
| STRING → INT/REAL | Parse, **slap** if invalid |
| LOGICAL → INT | `.TRUE.` = 1, `.FALSE.` = 0 |

---

## Data Flow

```
F9S> .load hello.f9s
    │
    ├─ handle_load_command()     → src/c/load.c
    │     ├─ extract_filename()  → parse ".load hello.f9s"
    │     └─ load_file()
    │           ├─ compile_f9s() → src/c/build.c
    │           │     ├─ parse_program()      → parser.c
    │           │     ├─ semantic_analysis()  → parser.c
    │           │     ├─ generate_code()      → codegen.c
    │           │     ├─ system("nasm ...")   → hello.obj
    │           │     └─ system("clang ...")  → hello.exe
    │           └─ system("hello.exe")        → run output
    │
    └─ [SUCCESS] Compiled hello.f9s -> hello.exe
       [LOG] Running hello.exe...
       42
       3.14159
```
