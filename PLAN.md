# PLAN — F9S (FORTRAN 95 Subset) Compiler

## Goal

Build **F9S**, a FORTRAN 95 Subset compiler targeting x86-64 Windows. Written primarily in NASM assembly with C for I/O and platform glue. Produces native `.exe` files from `.f9s` source.

---

## Phases

### Phase 1 — Calculator REPL ✅
- [x] REPL loop in `main.asm` with `F9S>` prompt
- [x] Integer arithmetic in pure NASM (`add`, `sub`, `mul`, `div`, `square`, `sqrt`)
- [x] SSE/AVX feature detection at startup (`enable.asm`)
- [x] Quake fast inverse sqrt fallback when SSE unavailable
- [x] FORTRAN 95 literal type detection (`int`, `real`, `complex`, `character`, `string`, `logical`)
- [x] Infix (`3 + 2`) and verb (`add 3 2`) input
- [x] Colored log helpers (`[LOG]`, `[WARN]`, `[ERROR]`, `[SUCCESS]`)
- [x] Source reorganised: `src/asm/`, `src/c/`, `src/include/` with per-directory sub-Makefiles
- [x] `int_fallback.c` — replaced pointer type-punning with `memcpy` (strict-aliasing safe)
- [x] `calc.c` — replaced fixed `buf[256]` with `malloc`-based dynamic buffer

### Phase 2 — Compiler Infrastructure ✅
- [x] AVL tree symbol table (`heap.c`, `symbol.h`)
- [x] Pat/Slap error handling system (`error_handler.c`)
- [x] Type conversion with warnings (`assignment.c`)
- [x] Recursive descent parser (`parser.c`)
- [x] AST construction and destruction (`ast.h`)
- [x] `.load` command — compile and run `.f9s` files
- [x] `.build` command — compile `.f9s` to `.exe` without running

### Phase 3 — Code Generation ✅
- [x] NASM x64 code generator (`codegen.c`)
- [x] Support for INTEGER, REAL (SSE2 double), LOGICAL, STRING types
- [x] `PRINT *` statement for all types
- [x] Windows x64 ABI compliance (shadow space, register args, SSE for floats)
- [x] Literal registry for strings and floats (`codegen_helpers.c`)

### Phase 4 — Control Flow ✅
- [x] `IF (condition) THEN` / `ELSE` / `END IF`
- [x] `DO` loop with integer control variable (`DO i = 1, n`) + optional step (`DO i = 0, n, 2`)
- [x] `STOP` statement
- [x] Condition expressions: `==`, `/=`, `<`, `>`, `<=`, `>=`

### Phase 5 — Procedures & Extended Control Flow
- [ ] `SUBROUTINE name(args)` / `END SUBROUTINE`
- [ ] `CALL` statement
- [ ] `FUNCTION` with return value
- [ ] Arguments passed by reference (FORTRAN 95 default)
- [ ] `ELSE IF (condition) THEN` chained branches
- [ ] Bare `ELSE` without requiring `THEN` on the same line

### Phase 6 — Arrays & Hashmaps ✅
- [x] Array declarations (`INTEGER :: arr(10)`)
- [x] Array indexing — `arr(i) = expr` (assign) and `arr(i)` (rvalue)
- [x] Implied DO loops in I/O — `PRINT *, (arr(i), i=1,n)`
- [x] Hashmap declarations (`HASHMAP :: map`)
- [x] Hashmap insert / lookup (`map(key) := value`, `map(key)`)
- [x] Integer keys; open-addressing, 64-slot BSS layout
- [x] Codegen: `_f9s_hm_insert` / `_f9s_hm_lookup` runtime emitted inline

### Phase 7 — Polish
- [ ] Missing features identified during testing
- [ ] Peephole optimisation pass on emitted NASM
- [ ] Clear documentation of F9S subset vs full FORTRAN 95

---

## Design Constraints

- **Maximum ASM, minimum C**: C used only for stdlib glue (stdio, type detection, fallback math)
- **No external assembler libraries**: all asm is pure NASM win64
- **Clang** for compiling C objects; NASM for asm objects; linked with `clang -m64`
- SSE2 assumed available on any modern x64; AVX checked at runtime
