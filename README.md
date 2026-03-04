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
| `sub 10 4` | `6` |
| `mul 6 7` | `42` |
| `div 9 3` | `3` |
| `square 5` | `25` |
| `sqrt 16` | `4` |

## F9S Language Features

F9S supports a subset of FORTRAN 95:

- **Types**: `INTEGER`, `REAL`, `LOGICAL`, `CHARACTER`, `STRING`, `COMPLEX`
- **Statements**: `PROGRAM`/`END PROGRAM`, `PRINT *`, assignments, `STOP`
- **Arithmetic operators**: `+`, `-`, `*`, `/`
- **Comparison operators**: `==`, `/=`, `<`, `>`, `<=`, `>=`
- **Control flow**: `IF (condition) THEN` / `ELSE` / `END IF`, `DO` loop
- **Type conversion**: Automatic conversion with warnings (pat/slap system)

### Example Program (`examples/sample.f9s`)

```fortran
PROGRAM sample
    INTEGER total := 0

    INTEGER n := 5
    INTEGER total := 0

    ! DO loop — sum 1..n
    DO i = 1, n
        total = total + i
    END DO

    PRINT *, total          ! 15

    ! IF / ELSE / END IF
    IF (total > 10) THEN
        PRINT *, total
    ELSE
        PRINT *, 0
    END IF

    ! Comparison operators
    IF (n == 5) THEN
        PRINT *, 1
    END IF

    IF (n /= 99) THEN
        PRINT *, 1
    END IF

    IF (n <= 5) THEN
        PRINT *, 1
    END IF

    IF (n >= 5) THEN
        PRINT *, 1
    END IF

    IF (n < 10) THEN
        PRINT *, 1
    END IF

    IF (n > 0) THEN
        PRINT *, 1
    END IF

    STOP
END PROGRAM sample
```

Compile and run:
```
F9S> .load examples\sample.f9s
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
! DO i = start, stop
DO i = 1, 10
    PRINT *, i
END DO

! DO i = start, stop, step
DO j = 0, 20, 2
    PRINT *, j
END DO
```

#### STOP

```fortran
STOP          ! terminate program immediately
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
- **Comparison operators**: `==`, `/=`, `<`, `>`, `<=`, `>=`
- **Colored logging**: `[LOG]` white, `[WARN]` yellow, `[ERROR]` red, `[SUCCESS]` green
- **Pat/Slap diagnostics**: Warnings (pat) and errors (slap) with summary counts

## Source Layout

```
src/
  asm/
    main.asm        — entry point, F9S REPL loop
    int.asm         — integer arithmetic (add/sub/mul/div/square/sqrt)
    enable.asm      — CPUID/XGETBV SSE+AVX feature detection
    Makefile
  c/
    build.c         — compile .f9s to .exe (compile_f9s, build_file)
    load.c          — compile and run .f9s (handle_load_command, handle_build_command)
    parser.c        — recursive descent parser
    codegen.c       — NASM x64 code generator
    heap.c          — AVL tree symbol table
    error_handler.c — pat/slap diagnostic system
    assignment.c    — type conversion
    calc.c          — calculator expression parser
    input.c         — fgets/fputs stdio helpers
    int_fallback.c  — Quake fast inverse sqrt fallback
    log.c           — ANSI colored log helpers
    types.c         — FORTRAN 95 type detection
    codegen_helpers.c — literal registry for codegen
    Makefile
  include/
    build.h, load.h, parser.h, codegen.h, codegen_helpers.h
    symbol.h, error_handler.h, ast.h, assignment.h
    calc.h, int.h, log.h, types.h
```

## Project Scale

F9S is a small, purpose-built compiler — not a full Fortran 95 implementation like GNU Fortran.
The entire source (C, headers, assembly, and makefiles) is under 3,000 lines:

```
Language          files   blank   comment   code
-------------------------------------------------
C                    13     344       295   2417
C/C++ Header         14      76       155    219
Assembly              4      23        38    198
make                  2      10         5     17
-------------------------------------------------
SUM:                 33     453       493   2851
```

*(generated with [cloc](https://github.com/AlDanial/cloc) v2.06)*
