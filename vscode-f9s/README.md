# F9S — VS Code Extension

Language support for **F9S** (FORTRAN 95 Subset) source files (`.f9s`).

## Features

### Syntax Highlighting
Full TextMate grammar covering:
- Keywords: `PROGRAM`, `END PROGRAM`, `PRINT`, `ASSIGN`
- Type keywords: `INTEGER`, `REAL`, `LOGICAL`, `CHARACTER`, `STRING`, `COMPLEX`
- Logical literals: `.TRUE.`, `.FALSE.`
- Numeric literals (integer and real/float)
- String literals (double-quoted)
- Comments (`!` to end of line)
- Arithmetic and assignment operators (`:=`, `=`, `+`, `-`, `*`, `/`)

### IntelliSense
- **Hover** — hover over any keyword for inline documentation and usage examples
- **Auto-complete** — keyword completions with documentation, plus variables declared in the current file

### Diagnostics
Real-time squiggles for:
- Duplicate `PROGRAM` statements
- `END PROGRAM` name mismatch
- `PRINT` without an argument
- Leading `:=` without a variable

### Commands
| Command | Keyboard | Description |
|---|---|---|
| **F9S: Run Current File** | — | Invokes `.load <file>` in the F9S REPL |
| **F9S: Build Current File** | — | Invokes `.build <file>` in the F9S REPL |

Both commands appear in the editor title bar and right-click context menu when a `.f9s` file is open.

### Snippets
| Prefix | Inserts |
|---|---|
| `program` | `PROGRAM … END PROGRAM` block |
| `int` | `INTEGER var := 0` |
| `real` | `REAL var := 0.0` |
| `logical` | `LOGICAL flag := .TRUE.` |
| `string` | `STRING var := "text"` |
| `char` | `CHARACTER var := "x"` |
| `complex` | `COMPLEX var := (re,im)` |
| `print` | `PRINT *, expr` |
| `printv` | `PRINT var` |
| `prints` | `PRINT *, "text"` |
| `assign` | `ASSIGN var, value` |
| `hello` | Hello World program |
| `f9sfull` | Full typed example |

### Build Task Provider
A built-in `f9s` task type is registered so you can add tasks to `tasks.json`:

```json
{
  "type": "f9s",
  "command": ".load",
  "file": "${file}",
  "label": "Run F9S file"
}
```

## Configuration

| Setting | Default | Description |
|---|---|---|
| `f9s.replExecutable` | `bin\program.exe` | Path to the F9S REPL executable |
| `f9s.workspaceRoot` | *(workspace folder)* | Override the working directory for the compiler |

## Requirements

- The F9S compiler must be built (`make`) before using the Run/Build commands.
- NASM and Clang must be in PATH for compilation to work.

## Development

```cmd
cd vscode-f9s
npm install
npm run compile
```

To launch the extension in a new Extension Development Host window, press **F5** inside the `vscode-f9s` folder (with the extension development launch config below in `.vscode/launch.json`).
