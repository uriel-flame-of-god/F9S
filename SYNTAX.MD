# F9S Syntax

This documents the F9S grammar as implemented in the compiler, noting where it diverges from standard Fortran 95 and from the VS Code grammar shipped in this repo.

## Big differences vs Fortran 95
- One program unit per file; every file must start with `PROGRAM <name>` and end with `END PROGRAM [name]` ([src/c/parser.c#L900-L939](src/c/parser.c#L900-L939)).
- No implicit typing rules; variables appear only via typed declarations or `ASSIGN` statements. Loop counters are implicitly `INTEGER` ([src/c/parser.c#L828-L874](src/c/parser.c#L828-L874), [src/c/parser.c#L720-L776](src/c/parser.c#L720-L776)).
- Assignment uses `:=` inside declarations; plain `=` is only accepted in `DO` headers. Traditional Fortran `=` assignments are not supported ([src/c/parser.c#L828-L874](src/c/parser.c#L828-L874), [src/c/parser.c#L720-L776](src/c/parser.c#L720-L776)).
- `PRINT` takes a single expression; no `*`, no format lists ([src/c/parser.c#L876-L900](src/c/parser.c#L876-L900)).
- Control flow is limited to `IF`/`ELSE`/`END IF`, `DO ... END DO`, and `STOP`—no `ELSE IF`, `SELECT CASE`, `GOTO`, procedures, or functions ([src/c/parser.c#L600-L676](src/c/parser.c#L600-L676), [src/c/parser.c#L720-L776](src/c/parser.c#L720-L776)).
- Comments use `!` to end of line; there is no continuation syntax and keywords are case-insensitive ([src/c/parser.c#L60-L116](src/c/parser.c#L60-L116)).

## Lexical rules
- **Identifiers:** `A-Za-z_` followed by alphanumerics/underscore; case-insensitive keyword matching ([src/c/parser.c#L60-L116](src/c/parser.c#L60-L116)).
- **Comments & whitespace:** `!` starts a comment to end of line; spaces/tabs/carriage returns are skipped, but newlines delimit statements ([src/c/parser.c#L60-L116](src/c/parser.c#L60-L116), [src/c/parser.c#L608-L642](src/c/parser.c#L608-L642)).
- **String literals:** single or double quoted; escapes are not interpreted—bytes are taken literally until the matching quote ([src/c/parser.c#L130-L176](src/c/parser.c#L130-L176)).
- **Logical literals:** `.TRUE.` and `.FALSE.` (case-insensitive) ([src/c/parser.c#L130-L190](src/c/parser.c#L130-L190)).
- **Complex literals:** `(re,im)` is recognized lexically, but is currently coerced into a real literal during parsing (imaginary part is ignored in codegen) ([src/c/parser.c#L130-L214](src/c/parser.c#L130-L214), [src/c/parser.c#L400-L458](src/c/parser.c#L400-L458)).
- **Numeric literals:** optional sign, decimal integers, and reals with optional exponent; no kind suffixes ([src/c/parser.c#L130-L214](src/c/parser.c#L130-L214)).

## Expressions
- Binary arithmetic `+ - * /` with standard precedence and left associativity ([src/c/parser.c#L560-L616](src/c/parser.c#L560-L616)).
- Unary minus folds into literals when possible ([src/c/parser.c#L520-L558](src/c/parser.c#L520-L558)).
- Comparisons allow a single relational operator; chained comparisons are not parsed ([src/c/parser.c#L616-L650](src/c/parser.c#L616-L650)).
- Parentheses group subexpressions; variables and literals are primary expressions ([src/c/parser.c#L400-L520](src/c/parser.c#L400-L520)).

## Statements
- **Program header/footer:** `PROGRAM name` … `END PROGRAM [name]` ([src/c/parser.c#L900-L939](src/c/parser.c#L900-L939)).
- **Typed declaration + initialization:** `INTEGER|REAL|STRING|LOGICAL|COMPLEX <name> := <expr>`; declaration is required before use. Multiple variables per line are not supported ([src/c/parser.c#L828-L854](src/c/parser.c#L828-L854)).
- **Implicit declaration via assignment:** `ASSIGN <name>, <expr>` introduces or updates a variable with inferred type ([src/c/parser.c#L856-L874](src/c/parser.c#L856-L874)).
- **Print:** `PRINT <expr>`; prints strings, numbers, or logical values ([src/c/parser.c#L876-L900](src/c/parser.c#L876-L900)).
- **Conditional:** `IF (<comparison or expr>) THEN` newline-separated body, optional `ELSE`, terminated by `END IF` ([src/c/parser.c#L624-L688](src/c/parser.c#L624-L688)).
- **Loop:** `DO <var> = <start>, <end> [, <step>]` newline-separated body, terminated by `END DO`. Loop var becomes `INTEGER` automatically ([src/c/parser.c#L690-L776](src/c/parser.c#L690-L776)).
- **Stop:** `STOP` terminates execution ([src/c/parser.c#L876-L900](src/c/parser.c#L876-L900)).

### Statement separation
- Newlines delimit statements. Trailing blank lines are skipped. There is no multi-statement per line syntax ([src/c/parser.c#L608-L642](src/c/parser.c#L608-L642)).

## Planned but not yet implemented
These features are on the roadmap (see [PLAN.md](PLAN.md)) and will be added in future phases:

- **Arrays** (Phase 6): `INTEGER :: arr(10)`, array indexing, implied DO loops in I/O.
- **Hashmaps** (Phase 6): `HASHMAP :: map`, insert/lookup with `map(key) := value` / `map(key)`, string and integer keys.
- **Subroutines & functions** (Phase 5): `SUBROUTINE name(args)` / `END SUBROUTINE`, `CALL`, `FUNCTION`, pass-by-reference.
- **`ELSE IF`** (Phase 5): `ELSE IF (condition) THEN` chained branches inside `IF` blocks.

## Permanently omitted Fortran 95 features
These are outside the F9S scope by design:
- No implicit typing by first letter; no `IMPLICIT` statements.
- No derived types, `PARAMETER`, `DATA` statements, or format descriptors.
- No `SELECT CASE`, `GOTO`, `CONTINUE`, or cycle/exit controls.
- No `MODULE`/`USE`, kind parameters, or I/O units.

## VS Code grammar caveats
The TextMate grammar in the extension highlights some constructs that the parser does not accept—e.g., `PRINT *, expr` and bare `PRINT` with comma, or `CHARACTER` as a type ([vscode-f9s/syntaxes/f9s.tmLanguage.json](vscode-f9s/syntaxes/f9s.tmLanguage.json)). When in doubt, prefer the forms shown above that match the compiler.

## Minimal example
```fortran
PROGRAM demo
    INTEGER x := 1
    REAL    y := 2.5
    ASSIGN z, x + y

    IF (z > 3) THEN
        PRINT z
    ELSE
        PRINT 0
    END IF

    DO i = 1, 3
        PRINT i
    END DO

    STOP
END PROGRAM demo
```

Use `make` then `.load file.f9s` in the REPL to compile and run.
