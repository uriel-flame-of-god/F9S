# DOCS — F9S API and Architecture Reference

This document provides a complete API reference and architecture guide for the
F9S compiler codebase.  For the project roadmap see `PLAN.md`; for low-level
implementation notes see `IMPL.md`.

---

## Table of Contents

1. [Compilation Pipeline](#1-compilation-pipeline)
2. [AST Node Types](#2-ast-node-types)
3. [Symbol Table API](#3-symbol-table-api)
4. [Type System API](#4-type-system-api)
5. [Parser API](#5-parser-api)
6. [Code Generator API](#6-code-generator-api)
7. [Codegen Helpers API](#7-codegen-helpers-api)
8. [Diagnostic API (Pat/Slap)](#8-diagnostic-api-patslap)
9. [Assignment & Type Conversion API](#9-assignment--type-conversion-api)
10. [REPL Commands API](#10-repl-commands-api)
11. [Calculator API](#11-calculator-api)
12. [Integer Arithmetic API](#12-integer-arithmetic-api)
13. [Logging API](#13-logging-api)
14. [Label Allocator API](#14-label-allocator-api)
15. [F9S Language Reference](#15-f9s-language-reference)

---

## 1. Compilation Pipeline

```
.f9s source file
      │
      ▼
parse_program()         → struct ast_node *    (parser.c)
      │
      ▼
semantic_analysis()     → int 0/-1             (parser.c)
      │
      ▼
generate_code()         → .asm file            (codegen.c)
      │
      ▼
nasm -f win64           → .obj file
      │
      ▼
clang -m64              → .exe file
```

The top-level entry for the pipeline is `compile_f9s()` in `build.c`.

---

## 2. AST Node Types

Defined in `src/include/ast.h`.

### `enum ast_node_type`

| Enumerator | Description |
|---|---|
| `AST_PROGRAM` | Root node: `PROGRAM name … END PROGRAM` |
| `AST_ASSIGN_EXPLICIT` | Typed declaration: `INTEGER x := 42` |
| `AST_ASSIGN_IMPLICIT` | Inferred assignment: `x = 42` or `ASSIGN x, 42` |
| `AST_PRINT` | `PRINT *, expr` statement |
| `AST_BINARY_OP` | Arithmetic or comparison sub-expression |
| `AST_LITERAL` | Integer, real, string, logical, or complex literal |
| `AST_VARIABLE` | Variable reference (resolved in semantic pass) |
| `AST_IF` | `IF (cond) THEN … [ELSE …] END IF` |
| `AST_DO` | `DO i = start, end [, step] … END DO` |
| `AST_STOP` | `STOP` statement |
| `AST_ARRAY_DECL` | `INTEGER :: arr(10)` |
| `AST_ARRAY_ASSIGN` | `arr(i) = expr` |
| `AST_ARRAY_REF` | `arr(i)` as rvalue |
| `AST_HASHMAP_DECL` | `HASHMAP :: map` |
| `AST_HASHMAP_INSERT` | `map(key) := value` |
| `AST_IMPLIED_DO` | `(expr, var=start,end[,step])` inside `PRINT` |

### `enum binary_op`

| Enumerator | Operator |
|---|---|
| `OP_ADD` | `+` |
| `OP_SUB` | `-` |
| `OP_MUL` | `*` |
| `OP_DIV` | `/` |
| `OP_EQ`  | `==` |
| `OP_NE`  | `/=` |
| `OP_LT`  | `<` |
| `OP_GT`  | `>` |
| `OP_LE`  | `<=` |
| `OP_GE`  | `>=` |

### AST node allocation

```c
struct ast_node *ast_node_alloc(enum ast_node_type type);
void             ast_destroy(struct ast_node *node);
```

`ast_destroy` follows both child pointers and `next` (statement list) pointers,
freeing the entire subtree.

---

## 3. Symbol Table API

Defined in `src/include/symbol.h`, implemented in `src/c/heap.c`.

AVL-balanced BST keyed by variable name.  All operations are O(log n).

```c
struct symbol_table *symbol_table_create(void);
void                 symbol_table_destroy(struct symbol_table *table);

struct symbol *symbol_insert(struct symbol_table *table,
                             const char          *name,
                             enum type_kind        type,
                             void                *initial_value);

struct symbol *symbol_lookup(struct symbol_table *table,
                             const char          *name);
```

### `struct symbol`

```c
struct symbol {
    char           *name;
    enum type_kind  type;
    union {
        long long  int_value;
        double     real_value;
        char      *string_value;
        struct { double real; double imag; } complex_value;
    } value;
    struct symbol *left, *right;
    int            height;
};
```

### `initial_value` pointer types

| `type` | Pointer type |
|---|---|
| `TYPE_INT` / `TYPE_LOGICAL` | `long long *` |
| `TYPE_REAL` | `double *` |
| `TYPE_STRING` / `TYPE_CHARACTER` | `char *` (NUL-terminated) |
| `TYPE_COMPLEX` | `struct { double real; double imag; } *` |
| `NULL` | zero-initialised |

---

## 4. Type System API

Defined in `src/include/types.h`, implemented in `src/c/types.c`.

### `enum type_kind`

| Enumerator | Meaning |
|---|---|
| `TYPE_INT` | 64-bit signed integer |
| `TYPE_CHARACTER` | Single character |
| `TYPE_STRING` | NUL-terminated character string |
| `TYPE_REAL` | 64-bit IEEE 754 double |
| `TYPE_COMPLEX` | Pair of doubles (real, imaginary) |
| `TYPE_LOGICAL` | Boolean (0 = `.FALSE.`, non-zero = `.TRUE.`) |
| `TYPE_UNKNOWN` | Not yet inferred |

### Functions

```c
int         detect_type(const char *s);
const char *type_name(int kind);
int         parse_string(const char *s, struct string_type *out);
```

`detect_type` inspects raw source tokens and returns a `type_kind`.
Precedence: `LOGICAL` → `CHARACTER`/`STRING` → `COMPLEX` → `REAL` → `INT` → `UNKNOWN`.

---

## 5. Parser API

Defined in `src/include/parser.h`, implemented in `src/c/parser.c`.

```c
struct ast_node *parse_program(const char *filename,
                               char      **source_buffer);

int semantic_analysis(struct ast_node    *ast,
                      struct symbol_table *table);
```

- `parse_program` tokenises and builds the AST.  Returns NULL on fatal parse error.
- `semantic_analysis` resolves `AST_VARIABLE` nodes by filling their `.variable.symbol` pointers from `table`.
- Both functions use the pat/slap diagnostic system; callers should check `slap_occurred()` after each call.

---

## 6. Code Generator API

Defined in `src/include/codegen.h`, implemented in `src/c/codegen.c`.

```c
int generate_code(struct ast_node *ast, const char *output_filename);
```

Performs three sub-passes:
1. **Data pass** — emit `.data` section (real constants `_rcN`, string literals `_strN`).
2. **BSS pass** — emit `.bss` section (scalar vars `var_<name>`, arrays `arr_<name>`, hashmaps `hm_<name>`).
3. **Text pass** — emit `.text` section (`_main` + all statement handlers).

Returns 0 on success, -1 on error.

### `struct codegen_context`

```c
struct codegen_context {
    FILE               *output;
    struct symbol_table *table;
    int                 label_counter;
};
```

This struct is internal and created/destroyed inside `generate_code`.

---

## 7. Codegen Helpers API

Defined in `src/include/codegen_helpers.h`, implemented in `src/c/codegen_helpers.c`.

### Data section emission

```c
int cg_emit_data_literals(struct ast_node *root, FILE *out);
int cg_real_label_id(double value);

int cg_emit_data_strings(struct ast_node *root, FILE *out);
int cg_string_label_id(const char *value);
```

### BSS section emission

```c
void cg_emit_bss_vars(struct ast_node *root, FILE *out);
```

Emits:
- `var_<name> resq 1` for scalar variables and DO loop control variables
- `arr_<name> resq <size>` for array declarations
- `hm_<name>  resb 2048` for hashmap declarations (64 slots × 32 bytes)

### Array and hashmap registries

```c
void           cg_register_array(const char *name, int size, enum type_kind elem_type);
void           cg_register_hashmap(const char *name);
int            cg_is_array(const char *name);
int            cg_is_hashmap(const char *name);
enum type_kind cg_array_elem_type(const char *name);
int            cg_hashmap_count(void);
```

### Hashmap runtime emission

```c
void cg_emit_hashmap_runtime(FILE *out);
```

Writes `_f9s_hm_insert` and `_f9s_hm_lookup` as inline NASM procedures.

**`_f9s_hm_insert`** — `rcx` = table base, `rdx` = key (`int64`), `r8` = value (`int64`)

**`_f9s_hm_lookup`** — `rcx` = table base, `rdx` = key (`int64`) → `rax` = value (0 if not found)

Both use 64-slot open-addressing with `hash = key & 63` and linear probing.

### Lifecycle

```c
void cg_helpers_reset(void);
```

Must be called before each file compilation to clear all registries.

---

## 8. Diagnostic API (Pat/Slap)

Defined in `src/include/error_handler.h`, implemented in `src/c/error_handler.c`.

```c
void pat(const char *format, ...);    /* warning */
void slap(const char *format, ...);   /* fatal error */

int  slap_occurred(void);             /* 1 if any slap since last reset */
void reset_diagnostics(void);         /* clear counters and flag */
void print_pat_summary(void);         /* print "N pats" if > 0 */

void set_error_context(const struct error_context *ctx);
void clear_error_context(void);
```

Both `pat` and `slap` accept `printf`-style format strings. When a source
context is attached via `set_error_context`, diagnostics include the file name
and line number prefix.

### Global counters

```c
extern int g_pat_count;   /* incremented by pat() */
extern int g_slap_count;  /* incremented by slap() */
```

---

## 9. Assignment & Type Conversion API

Defined in `src/include/assignment.h`, implemented in `src/c/assignment.c`.

```c
int handle_explicit_assignment(struct symbol_table *table,
                               const char          *var_name,
                               enum type_kind        declared_type,
                               void                *value,
                               enum type_kind        value_type);

int handle_implicit_assignment(struct symbol_table *table,
                               const char          *var_name,
                               void                *value,
                               enum type_kind        value_type);

int convert_value(void          *dest,
                  enum type_kind  dest_type,
                  const void    *src,
                  enum type_kind  src_type,
                  int            *pat_count);

enum type_kind get_literal_type(const char *literal);
```

### Conversion policy

| From → To | Result |
|---|---|
| INT → REAL | Widen, issue `pat` |
| REAL → INT | Truncate, issue `pat` |
| COMPLEX → INT/REAL | Drop imaginary part, issue `pat` |
| LOGICAL → INT | 0 or 1, issue `pat` |
| STRING → INT/REAL | Parse numerically; `slap` if invalid |
| Any → STRING | Always `slap` |

Returns 0 on success, -1 if `slap()` was called.

---

## 10. REPL Commands API

Defined in `src/include/load.h`, implemented in `src/c/load.c`.

```c
int handle_load_command(const char *line);
int handle_build_command(const char *line);
int load_file(const char *filename, struct symbol_table *table);
```

| Return value | Meaning |
|---|---|
| `0` | Command handled successfully |
| `-1` | Not a recognised command (line does not start with `.load`/`.build`) |
| `-2` | Compilation or execution error |

---

## 11. Calculator API

Defined in `src/include/calc.h`, implemented in `src/c/calc.c`.

```c
int is_exit(const char *s);
int compute(const char *line, int features, long long *out);
```

`compute` supports:
- Infix: `3 + 2`, `10 / 4`
- Prefix verb: `add 3 5`, `sub 10 4`, `mul 6 7`, `div 9 3`, `square 5`, `sqrt 16`

`features` bit 0 enables the NASM `sqrt_int`; otherwise `sqrt_int_fallback` is used.

Return codes: `0` = success, `-2` = SSE required, `-3` = divide by zero, `-4` = parse error, `-5` = sqrt of negative.

---

## 12. Integer Arithmetic API

Defined in `src/include/int.h`, implemented in `src/asm/int.asm` and `src/c/int_fallback.c`.

```c
long long add_int(long long a, long long b);
long long sub_int(long long a, long long b);
long long mul_int(long long a, long long b);
long long div_int(long long a, long long b);   /* returns 0 for b == 0 */
long long square_int(long long a);
long long sqrt_int(long long a);               /* SSE2; 0 for negative */
long long sqrt_int_fallback(long long a);      /* Quake fast inverse sqrt */
```

---

## 13. Logging API

Defined in `src/include/log.h`, implemented in `src/c/log.c`.

```c
void log_info(const char *msg);     /* white   [LOG]     */
void log_warn(const char *msg);     /* yellow  [WARN]    */
void log_error(const char *msg);    /* red     [ERROR]   */
void log_success(const char *msg);  /* green   [SUCCESS] */
```

All functions write to `stdout` using ANSI escape codes.

---

## 14. Label Allocator API

Defined in `src/include/control.h`, implemented in `src/asm/control.asm`.

```c
int  label_alloc(void);   /* returns 0, 1, 2, … */
void label_reset(void);   /* reset to 0 */
```

Used by `codegen.c` to generate unique branch labels per translation unit.

---

## 15. F9S Language Reference

### Program structure

```fortran
PROGRAM name
    ! statements
END PROGRAM name
```

### Types

| Keyword | `type_kind` | Storage |
|---|---|---|
| `INTEGER` | `TYPE_INT` | 64-bit signed |
| `REAL` | `TYPE_REAL` | 64-bit double |
| `LOGICAL` | `TYPE_LOGICAL` | 64-bit (0/1) |
| `CHARACTER` | `TYPE_CHARACTER` | Single char |
| `STRING` | `TYPE_STRING` | NUL-terminated |
| `COMPLEX` | `TYPE_COMPLEX` | Two doubles |

### Statements

#### Variable declaration (explicit)
```fortran
INTEGER x := 42
REAL    pi := 3.14
LOGICAL flag := .TRUE.
```

#### Assignment (implicit)
```fortran
x = x + 1
y = 2.718
```

#### Array declaration and use
```fortran
INTEGER :: arr(10)
arr(1) = 100
PRINT *, arr(1)
PRINT *, (arr(i), i=1,10)
```

Arrays are 1-based.  Only integer element types are currently emitted with the
full codegen path; REAL arrays load via `movsd`.

#### Hashmap declaration and use
```fortran
HASHMAP :: map
map(1) := 42
PRINT *, map(1)
```

Hashmaps use 64-slot open-addressing with integer keys.
Maximum 64 entries per hashmap (slots fill up with linear probing).

#### PRINT

```fortran
PRINT *, expr
PRINT *, (expr, i=start, end)
PRINT *, (expr, i=start, end, step)
```

#### IF / ELSE / END IF

```fortran
IF (condition) THEN
    ! then body
ELSE
    ! else body (optional)
END IF
```

#### DO loop

```fortran
DO i = 1, n
    ! body
END DO

DO i = 0, 20, 2
    ! body (step = 2)
END DO
```

#### STOP

```fortran
STOP
```

Calls `ExitProcess(0)` in the generated code.

### Expressions

```
expr → term (('+' | '-') term)*
term → factor (('*' | '/') factor)*
factor → INT_LIT | REAL_LIT | STRING_LIT | LOGICAL_LIT | COMPLEX_LIT
       | IDENTIFIER
       | IDENTIFIER '(' expr ')'    ← array ref or hashmap lookup
       | '(' expr ')'
```

### Comparison operators

| Operator | Meaning |
|---|---|
| `==` | Equal |
| `/=` | Not equal |
| `<`  | Less than |
| `>`  | Greater than |
| `<=` | Less than or equal |
| `>=` | Greater than or equal |

### Comments

```fortran
! This is a comment — rest of line is ignored
```
