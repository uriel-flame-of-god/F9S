# F9S — FORTRAN 95 Subset

A compiler for **F9S** (FORTRAN 95 Subset), written in C and x86-64 NASM assembly.
F9S targets Windows x64 and compiles `.f9s` source files to native executables.

## Requirements

- [NASM](https://nasm.us) (in PATH)
- [Clang](https://clang.llvm.org) (in PATH)
- Windows x64

## Build

```cmd
make
```

Clean and rebuild:

```cmd
make clean && make
```

## Run

```cmd
bin\program.exe
```

## REPL Commands

At the `F9S>` prompt:

| Command | Description |
|---|---|
| `.load file.f9s` | Compile and run a F9S source file |
| `.build file.f9s` | Compile a F9S source file to `.exe` (no run) |
| `exit` / `quit` | Exit the REPL |

### Calculator Mode

The REPL also supports basic calculator expressions:

| Input | Result |
|---|---|
| `3 + 2` | `5` |
| `10 - 4` | `6` |
| `6 * 7` | `42` |
| `9 / 3` | `3` |
| `add 3, 2` | `5` |
| `square 5` | `25` |
| `sqrt 16` | `4` |

## F9S Language Features

F9S supports a subset of FORTRAN 95:

- **Types**: `INTEGER`, `REAL`, `LOGICAL`, `CHARACTER`, `STRING`, `COMPLEX`
- **Statements**: `PROGRAM`/`END PROGRAM`, `PRINT *`, assignments, `STOP`
- **Arithmetic operators**: `+`, `-`, `*`, `/`
- **Comparison operators**: `==`, `/=`, `<`, `>`, `<=`, `>=`
- **Control flow**: `IF (condition) THEN` / `ELSE` / `END IF`, `DO` loop
- **Arrays**: `INTEGER :: arr(10)`, `arr(i) = expr`, `arr(i)` as rvalue
- **Hashmaps**: `HASHMAP :: map`, `map(key) := value`, `map(key)` as rvalue
- **Implied DO**: `PRINT *, (arr(i), i=1,n)` in `PRINT *` statements
- **Type conversion**: Automatic conversion with warnings (pat/slap system)

### Example: Control Flow (`examples/sample.f9s`)

```fortran
PROGRAM sample
    INTEGER n := 5
    INTEGER total := 0

    DO i = 1, n
        total = total + i
    END DO

    PRINT *, total          ! 15

    IF (total > 10) THEN
        PRINT *, total
    ELSE
        PRINT *, 0
    END IF

    STOP
END PROGRAM sample
```

### Example: Arrays and Hashmaps (`examples/phase6_test.f9s`)

```fortran
PROGRAM phase6_test
    INTEGER :: arr(5)

    DO i = 1, 5
        arr(i) = i * 10
    END DO

    PRINT *, (arr(i), i=1,5)

    HASHMAP :: scores
    scores(1) := 100
    scores(2) := 200
    PRINT *, scores(1)
    PRINT *, scores(2)

    STOP
END PROGRAM phase6_test
```

Compile and run:
```
F9S> .load examples\phase6_test.f9s
```

### Control Flow

#### IF statement

```fortran
IF (x > 0) THEN
    PRINT *, x
END IF

IF (x == 0) THEN
    PRINT *, 0
ELSE
    PRINT *, x
END IF
```

#### DO loop

```fortran
DO i = 1, 10
    PRINT *, i
END DO

DO j = 0, 20, 2
    PRINT *, j
END DO
```

#### Arrays

```fortran
INTEGER :: arr(10)
arr(1) = 42
PRINT *, arr(1)
PRINT *, (arr(i), i=1,5)
```

#### Hashmaps

```fortran
HASHMAP :: map
map(1) := 100
map(2) := 200
PRINT *, map(1)
```

#### STOP

```fortran
STOP
```

#### Comparison operators

| Operator | Meaning |
|---|---|
| `==` | equal |
| `/=` | not equal |
| `<` | less than |
| `>` | greater than |
| `<=` | less than or equal |
| `>=` | greater than or equal |

## Features

- **SSE/AVX detection** at startup — hardware `sqrt` when SSE available
- **Integer arithmetic** in pure NASM (`int.asm`)
- **Type detection** for FORTRAN 95 literals
- **Control flow**: `IF`/`THEN`/`ELSE`/`END IF`, `DO` loops, `STOP`
- **Arrays**: fixed-size, 1-based indexing, integer and real elements
- **Hashmaps**: 64-slot open-addressing BSS tables with integer keys
- **Implied DO**: `(expr, i=start,end[,step])` syntax in `PRINT *`
- **Colored logging**: `[LOG]` white, `[WARN]` yellow, `[ERROR]` red, `[SUCCESS]` green
- **Pat/Slap diagnostics**: Warnings (pat) and errors (slap) with summary counts

## Source Layout

```
src/
  asm/
    main.asm        — entry point, F9S REPL loop
    int.asm         — integer arithmetic (add/sub/mul/div/square/sqrt)
    enable.asm      — CPUID/XGETBV SSE+AVX feature detection
    control.asm     — monotonic label allocator for codegen
    Makefile
  c/
    build.c         — compile .f9s to .exe (compile_f9s, build_file)
    load.c          — compile and run .f9s (handle_load_command, handle_build_command)
    parser.c        — recursive descent parser + semantic analyser
    codegen.c       — NASM x64 code generator
    codegen_helpers.c — literal/array/hashmap registries
    heap.c          — AVL tree symbol table
    error_handler.c — pat/slap diagnostic system
    assignment.c    — type conversion
    calc.c          — calculator expression parser
    input.c         — fgets/fputs stdio helpers
    int_fallback.c  — Quake fast inverse sqrt fallback
    log.c           — ANSI colored log helpers
    types.c         — FORTRAN 95 type detection
    Makefile
  include/
    ast.h, symbol.h, types.h, parser.h
    codegen.h, codegen_helpers.h, control.h
    build.h, load.h, assignment.h
    calc.h, int.h, log.h, error_handler.h
examples/
  sample.f9s        — control flow demo
  phase6_test.f9s   — arrays + hashmaps + implied DO demo
```

## Project Scale

F9S is a small, purpose-built compiler — not a full Fortran 95 implementation.
The entire source (C, headers, assembly, and makefiles) is under 3,500 lines.
