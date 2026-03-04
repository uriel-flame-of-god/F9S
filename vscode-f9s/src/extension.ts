import * as vscode from 'vscode';
import * as path from 'path';
import * as fs from 'fs';

// ─────────────────────────────────────────────────────────────────────────────
//  Keyword documentation used by the hover and completion providers
// ─────────────────────────────────────────────────────────────────────────────
const KEYWORD_DOCS: Record<string, string> = {
    PROGRAM:   '**PROGRAM** *name*\n\nBegins a named F9S program block.\n\n```f9s\nPROGRAM hello\n    ...\nEND PROGRAM hello\n```',
    END:       '**END** / **END PROGRAM** *name* / **END IF** / **END DO**\n\nCloses the nearest open block.',
    PRINT:     '**PRINT** \\*, *expr*\n\nPrints an expression to stdout.\n\n```f9s\nPRINT *, n\nPRINT *, "text"\n```',
    ASSIGN:    '**ASSIGN** *var*, *value*\n\nImplicit variable assignment.\n\n```f9s\nASSIGN x, 42\n```',
    INTEGER:   '**INTEGER** *var* := *value*\n\nDeclares a 64-bit signed integer variable.\n\n```f9s\nINTEGER n := 42\n```',
    REAL:      '**REAL** *var* := *value*\n\nDeclares a 64-bit double-precision floating-point variable.\n\n```f9s\nREAL pi := 3.14159\n```',
    LOGICAL:   '**LOGICAL** *var* := *.TRUE.* | *.FALSE.*\n\nDeclares a boolean variable.\n\n```f9s\nLOGICAL flag := .TRUE.\n```',
    CHARACTER: '**CHARACTER** *var* := *"x"*\n\nDeclares a single character variable.\n\n```f9s\nCHARACTER c := "A"\n```',
    STRING:    '**STRING** *var* := *"text"*\n\nDeclares a string variable.\n\n```f9s\nSTRING s := "Hello"\n```',
    COMPLEX:   '**COMPLEX** *var* := *(re,im)*\n\nDeclares a complex number variable.\n\n```f9s\nCOMPLEX z := (1.0, 2.0)\n```',
    '.TRUE.':  '**Logical literal** — boolean true.',
    '.FALSE.': '**Logical literal** — boolean false.',
    IF:        '**IF** *(condition)* **THEN**\n\nConditional block. Condition uses `==`, `/=`, `<`, `>`, `<=`, `>=`.\n\n```f9s\nIF (n == 0) THEN\n    PRINT *, "zero"\nELSE\n    PRINT *, n\nEND IF\n```',
    THEN:      '**THEN**\n\nOpens the body of an `IF` or `ELSE IF` block.',
    ELSE:      '**ELSE** / **ELSE IF** *(condition)* **THEN**\n\nAlternative branch of an `IF` block.\n\n```f9s\nELSE\n    PRINT *, "other"\n```',
    DO:        '**DO** *var* = *start*, *end* [, *step*]\n\nInteger-controlled loop.\n\n```f9s\nDO i = 1, 10\n    PRINT *, i\nEND DO\n```',
    STOP:      '**STOP**\n\nTerminates program execution immediately.\n\n```f9s\nSTOP\n```',
};

// Keywords used for completion
const KEYWORDS = [
    'PROGRAM', 'END PROGRAM', 'END IF', 'END DO', 'END',
    'PRINT', 'ASSIGN', 'STOP',
    'IF', 'THEN', 'ELSE IF', 'ELSE',
    'DO',
    'INTEGER', 'REAL', 'LOGICAL', 'CHARACTER', 'STRING', 'COMPLEX',
    '.TRUE.', '.FALSE.',
];

// ─────────────────────────────────────────────────────────────────────────────
//  Hover Provider
// ─────────────────────────────────────────────────────────────────────────────
class F9SHoverProvider implements vscode.HoverProvider {
    provideHover(
        document: vscode.TextDocument,
        position: vscode.Position,
        _token: vscode.CancellationToken,
    ): vscode.Hover | undefined {
        const range = document.getWordRangeAtPosition(position, /\.?[A-Za-z_][A-Za-z0-9_]*\.?/);
        if (!range) return undefined;

        const word = document.getText(range).toUpperCase();
        const doc = KEYWORD_DOCS[word];
        if (!doc) return undefined;

        return new vscode.Hover(new vscode.MarkdownString(doc), range);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Completion Provider
// ─────────────────────────────────────────────────────────────────────────────
class F9SCompletionProvider implements vscode.CompletionItemProvider {
    provideCompletionItems(
        document: vscode.TextDocument,
        _position: vscode.Position,
    ): vscode.CompletionItem[] {
        const items: vscode.CompletionItem[] = [];

        for (const kw of KEYWORDS) {
            const item = new vscode.CompletionItem(kw, vscode.CompletionItemKind.Keyword);
            const doc = KEYWORD_DOCS[kw];
            if (doc) {
                item.documentation = new vscode.MarkdownString(doc);
            }
            items.push(item);
        }

        // Collect identifiers already defined in the document
        const text = document.getText();
        const identPat = /\b(INTEGER|REAL|LOGICAL|CHARACTER|STRING|COMPLEX)\s+([A-Za-z_][A-Za-z0-9_]*)/gi;
        let m: RegExpExecArray | null;
        const seen = new Set<string>();
        while ((m = identPat.exec(text)) !== null) {
            const varName = m[2];
            if (!seen.has(varName)) {
                seen.add(varName);
                const vitem = new vscode.CompletionItem(varName, vscode.CompletionItemKind.Variable);
                vitem.detail = `${m[1].toUpperCase()} variable`;
                items.push(vitem);
            }
        }

        return items;
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Diagnostics — real-time syntax checking
// ─────────────────────────────────────────────────────────────────────────────
function diagnose(document: vscode.TextDocument, collection: vscode.DiagnosticCollection): void {
    if (document.languageId !== 'f9s') return;

    const diagnostics: vscode.Diagnostic[] = [];
    const lines = document.getText().split(/\r?\n/);

    let hasProgramStart = false;
    let hasProgramEnd   = false;
    let programName     = '';
    // Stack-based block tracking: each entry is { kind, line }
    const blockStack: Array<{ kind: 'IF' | 'DO', line: number }> = [];

    for (let i = 0; i < lines.length; i++) {
        const raw  = lines[i];
        const line = raw.trimStart();

        // Skip comment lines
        if (line.startsWith('!')) continue;

        // PROGRAM <name>
        const progMatch = /^PROGRAM\s+([A-Za-z_][A-Za-z0-9_]*)/i.exec(line);
        if (progMatch) {
            if (hasProgramStart) {
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(i, 0, i, raw.length),
                    'Duplicate PROGRAM statement.',
                    vscode.DiagnosticSeverity.Error,
                ));
            }
            hasProgramStart = true;
            programName = progMatch[1].toUpperCase();
        }

        // END PROGRAM [name]
        const endProgMatch = /^END\s+PROGRAM(?:\s+([A-Za-z_][A-Za-z0-9_]*))?/i.exec(line);
        if (endProgMatch) {
            hasProgramEnd = true;
            const endName = (endProgMatch[1] || '').toUpperCase();
            if (endName && programName && endName !== programName) {
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(i, 0, i, raw.length),
                    `END PROGRAM name '${endName}' does not match PROGRAM name '${programName}'.`,
                    vscode.DiagnosticSeverity.Warning,
                ));
            }
        }

        // END IF
        if (/^END\s+IF\b/i.test(line)) {
            const top = blockStack[blockStack.length - 1];
            if (!top || top.kind !== 'IF') {
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(i, 0, i, raw.length),
                    'END IF without a matching IF ... THEN.',
                    vscode.DiagnosticSeverity.Error,
                ));
            } else {
                blockStack.pop();
            }
        }

        // END DO
        else if (/^END\s+DO\b/i.test(line)) {
            const top = blockStack[blockStack.length - 1];
            if (!top || top.kind !== 'DO') {
                diagnostics.push(new vscode.Diagnostic(
                    new vscode.Range(i, 0, i, raw.length),
                    'END DO without a matching DO loop.',
                    vscode.DiagnosticSeverity.Error,
                ));
            } else {
                blockStack.pop();
            }
        }

        // Bare END
        else if (/^END\s*$/i.test(line)) {
            hasProgramEnd = true;
        }

        // IF (condition) THEN — opens a block
        if (/^(?:ELSE\s+)?IF\s*\(.*\)\s*THEN\b/i.test(line)) {
            // ELSE IF closes the previous IF and opens a new one — we don't
            // push a new block for ELSE IF, treat it as a continuation.
            if (!/^ELSE\s+IF/i.test(line)) {
                blockStack.push({ kind: 'IF', line: i });
            }
        }

        // DO i = start, end
        if (/^DO\b\s+[A-Za-z_][A-Za-z0-9_]*\s*=/i.test(line)) {
            blockStack.push({ kind: 'DO', line: i });
        }

        // PRINT without argument
        if (/^PRINT\s*$/i.test(line)) {
            diagnostics.push(new vscode.Diagnostic(
                new vscode.Range(i, 0, i, raw.length),
                'PRINT requires an expression or *, expr.',
                vscode.DiagnosticSeverity.Warning,
            ));
        }

        // := without a type keyword or identifier on left
        const badAssign = /^\s*:=/.exec(raw);
        if (badAssign) {
            diagnostics.push(new vscode.Diagnostic(
                new vscode.Range(i, 0, i, raw.length),
                'Assignment operator := without a variable on the left-hand side.',
                vscode.DiagnosticSeverity.Error,
            ));
        }
    }

    // Report any unclosed IF / DO blocks
    for (const blk of blockStack) {
        diagnostics.push(new vscode.Diagnostic(
            new vscode.Range(blk.line, 0, blk.line, lines[blk.line].length),
            `Unclosed ${blk.kind} block — missing END ${blk.kind}.`,
            vscode.DiagnosticSeverity.Warning,
        ));
    }

    // Check matching PROGRAM / END PROGRAM
    if (hasProgramStart && !hasProgramEnd) {
        diagnostics.push(new vscode.Diagnostic(
            new vscode.Range(0, 0, 0, 0),
            `PROGRAM '${programName}' has no matching END PROGRAM.`,
            vscode.DiagnosticSeverity.Warning,
        ));
    }

    collection.set(document.uri, diagnostics);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers — resolve compiler executable and workspace root
// ─────────────────────────────────────────────────────────────────────────────
function resolveExecutable(wsRoot: string): string | undefined {
    const cfg = vscode.workspace.getConfiguration('f9s');
    const exePath: string = cfg.get('replExecutable') || 'bin\\program.exe';
    const customRoot: string = cfg.get('workspaceRoot') || '';
    const base = customRoot || wsRoot;
    const full = path.isAbsolute(exePath) ? exePath : path.join(base, exePath);
    return fs.existsSync(full) ? full : undefined;
}

function getWorkspaceRoot(): string | undefined {
    const folders = vscode.workspace.workspaceFolders;
    return folders && folders.length > 0 ? folders[0].uri.fsPath : undefined;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Commands — F9S: Build / Run
// ─────────────────────────────────────────────────────────────────────────────
function registerCommands(context: vscode.ExtensionContext): void {
    context.subscriptions.push(
        vscode.commands.registerCommand('f9s.buildFile', async () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor || editor.document.languageId !== 'f9s') {
                vscode.window.showErrorMessage('F9S: No active .f9s file.');
                return;
            }
            await editor.document.save();
            const wsRoot = getWorkspaceRoot();
            if (!wsRoot) {
                vscode.window.showErrorMessage('F9S: No workspace folder found.');
                return;
            }
            const exe = resolveExecutable(wsRoot);
            if (!exe) {
                vscode.window.showErrorMessage(
                    `F9S: Compiler executable not found. Build the project first (make), then check the 'f9s.replExecutable' setting.`
                );
                return;
            }
            const filePath = editor.document.fileName;
            runF9SCommand(exe, wsRoot, `.build ${filePath}`);
        }),

        vscode.commands.registerCommand('f9s.runFile', async () => {
            const editor = vscode.window.activeTextEditor;
            if (!editor || editor.document.languageId !== 'f9s') {
                vscode.window.showErrorMessage('F9S: No active .f9s file.');
                return;
            }
            await editor.document.save();
            const wsRoot = getWorkspaceRoot();
            if (!wsRoot) {
                vscode.window.showErrorMessage('F9S: No workspace folder found.');
                return;
            }
            const exe = resolveExecutable(wsRoot);
            if (!exe) {
                vscode.window.showErrorMessage(
                    `F9S: Compiler executable not found. Build the project first (make), then check the 'f9s.replExecutable' setting.`
                );
                return;
            }
            const filePath = editor.document.fileName;
            runF9SCommand(exe, wsRoot, `.load ${filePath}`);
        }),
    );
}

let f9sTerminal: vscode.Terminal | undefined;

function runF9SCommand(exe: string, cwd: string, command: string): void {
    // Reuse or create a dedicated F9S terminal
    if (!f9sTerminal || f9sTerminal.exitStatus !== undefined) {
        f9sTerminal = vscode.window.createTerminal({
            name: 'F9S',
            cwd,
        });
    }
    f9sTerminal.show(true);
    // Send the command through stdin of the REPL via `echo command | exe`
    // On Windows cmd-compatible shell:
    f9sTerminal.sendText(`echo ${command} | "${exe}"`, true);
}

// ─────────────────────────────────────────────────────────────────────────────
//  Task Provider
// ─────────────────────────────────────────────────────────────────────────────
class F9STaskProvider implements vscode.TaskProvider {
    static taskType = 'f9s';

    private wsRoot: string;

    constructor(wsRoot: string) {
        this.wsRoot = wsRoot;
    }

    provideTasks(): vscode.Task[] {
        const exe = resolveExecutable(this.wsRoot) ?? path.join(this.wsRoot, 'bin', 'program.exe');
        const tasks: vscode.Task[] = [];

        const editor = vscode.window.activeTextEditor;
        const filePath = editor?.document.languageId === 'f9s'
            ? editor.document.fileName
            : '${file}';

        for (const [label, cmd] of [['Build F9S file', '.build'], ['Run F9S file', '.load']] as const) {
            const shellCmd = `echo ${cmd} ${filePath} | "${exe}"`;
            const taskDef: vscode.TaskDefinition = { type: F9STaskProvider.taskType, command: cmd };
            const task = new vscode.Task(
                taskDef,
                vscode.TaskScope.Workspace,
                label,
                'F9S',
                new vscode.ShellExecution(shellCmd, { cwd: this.wsRoot }),
                [],
            );
            task.group = cmd === '.build' ? vscode.TaskGroup.Build : undefined;
            tasks.push(task);
        }

        return tasks;
    }

    resolveTask(task: vscode.Task): vscode.Task | undefined {
        const def = task.definition;
        if (def.type !== F9STaskProvider.taskType) return undefined;

        const exe = resolveExecutable(this.wsRoot) ?? path.join(this.wsRoot, 'bin', 'program.exe');
        const file: string = def.file ?? '${file}';
        const cmd: string  = def.command ?? '.load';
        const shellCmd = `echo ${cmd} ${file} | "${exe}"`;

        return new vscode.Task(
            def,
            task.scope ?? vscode.TaskScope.Workspace,
            task.name,
            'F9S',
            new vscode.ShellExecution(shellCmd, { cwd: this.wsRoot }),
        );
    }
}

// ─────────────────────────────────────────────────────────────────────────────
//  Activate
// ─────────────────────────────────────────────────────────────────────────────
export function activate(context: vscode.ExtensionContext): void {
    // Diagnostics collection
    const diagCollection = vscode.languages.createDiagnosticCollection('f9s');
    context.subscriptions.push(diagCollection);

    // Run diagnostics on open and change
    if (vscode.window.activeTextEditor) {
        diagnose(vscode.window.activeTextEditor.document, diagCollection);
    }
    context.subscriptions.push(
        vscode.workspace.onDidOpenTextDocument(doc => diagnose(doc, diagCollection)),
        vscode.workspace.onDidChangeTextDocument(e => diagnose(e.document, diagCollection)),
        vscode.workspace.onDidCloseTextDocument(doc => diagCollection.delete(doc.uri)),
    );

    // Hover provider
    context.subscriptions.push(
        vscode.languages.registerHoverProvider({ language: 'f9s' }, new F9SHoverProvider()),
    );

    // Completion provider
    context.subscriptions.push(
        vscode.languages.registerCompletionItemProvider(
            { language: 'f9s' },
            new F9SCompletionProvider(),
            '.', // trigger on '.' for .TRUE. / .FALSE. etc.
        ),
    );

    // Commands
    registerCommands(context);

    // Task provider
    const wsRoot = getWorkspaceRoot();
    if (wsRoot) {
        context.subscriptions.push(
            vscode.tasks.registerTaskProvider(F9STaskProvider.taskType, new F9STaskProvider(wsRoot)),
        );
    }
}

export function deactivate(): void {
    if (f9sTerminal) {
        f9sTerminal.dispose();
    }
}
