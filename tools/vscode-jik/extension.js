"use strict";

const fs = require("fs");
const path = require("path");
const vscode = require("vscode");

const BUNDLED_INDEX_PATH = path.join(__dirname, "data", "stdlib-index.json");
const MODULE_IMPORT_RE = /^\s*use\s+"([^"]+)"(?:\s+as\s+([A-Za-z_][A-Za-z0-9_]*))?/gm;
const QUALIFIED_ACCESS_RE = /([A-Za-z_][A-Za-z0-9_]*)::([A-Za-z_][A-Za-z0-9_]*)?$/;

let bundledIndex = null;
let liveIndexCache = null;

function activate(context) {
  const selector = { language: "jik", scheme: "*" };

  bundledIndex = loadBundledIndex();
  liveIndexCache = null;

  context.subscriptions.push(
    vscode.workspace.onDidChangeConfiguration((event) => {
      if (event.affectsConfiguration("jik.stdlibPath")) {
        liveIndexCache = null;
      }
    })
  );

  context.subscriptions.push(
    vscode.languages.registerCompletionItemProvider(
      selector,
      {
        provideCompletionItems(document, position) {
          return provideQualifiedCompletions(document, position);
        }
      },
      ":"
    )
  );

  context.subscriptions.push(
    vscode.languages.registerHoverProvider(selector, {
      provideHover(document, position) {
        return provideQualifiedHover(document, position);
      }
    })
  );
}

function deactivate() {}

function provideQualifiedCompletions(document, position) {
  const line = document.lineAt(position.line).text.slice(0, position.character);
  const match = line.match(QUALIFIED_ACCESS_RE);
  if (!match) {
    return undefined;
  }

  const alias = match[1];
  const partial = match[2] || "";
  const imports = parseImports(document.getText());
  const moduleName = imports.get(alias);
  if (!moduleName) {
    return undefined;
  }

  const index = getStdlibIndex();
  const moduleInfo = index[moduleName];
  if (!moduleInfo || !Array.isArray(moduleInfo.symbols)) {
    return undefined;
  }

  const start = new vscode.Position(position.line, position.character - partial.length);
  const range = new vscode.Range(start, position);
  const items = [];

  for (const symbol of moduleInfo.symbols) {
    if (partial && !symbol.name.startsWith(partial)) {
      continue;
    }
    items.push(makeCompletionItem(moduleName, symbol, range));
  }

  return items;
}

function makeCompletionItem(moduleName, symbol, range) {
  const kind = completionKind(symbol.kind);
  const item = new vscode.CompletionItem(symbol.name, kind);
  const signature = symbol.signature || symbol.name;

  item.range = range;
  item.insertText = symbol.name;
  item.filterText = symbol.name;
  item.sortText = sortPrefix(symbol.kind) + symbol.name;
  item.detail = moduleName + "::" + signature;
  item.documentation = makeSymbolMarkdown(moduleName, symbol);

  return item;
}

function provideQualifiedHover(document, position) {
  const resolved = resolveQualifiedSymbolAtPosition(document, position);
  if (!resolved) {
    return undefined;
  }

  return new vscode.Hover(
    makeSymbolMarkdown(resolved.moduleName, resolved.symbol),
    resolved.range
  );
}

function completionKind(kind) {
  switch (kind) {
    case "function":
      return vscode.CompletionItemKind.Function;
    case "struct":
      return vscode.CompletionItemKind.Struct;
    case "enum":
      return vscode.CompletionItemKind.Enum;
    case "constant":
      return vscode.CompletionItemKind.Constant;
    default:
      return vscode.CompletionItemKind.Text;
  }
}

function sortPrefix(kind) {
  switch (kind) {
    case "function":
      return "1_";
    case "struct":
      return "2_";
    case "enum":
      return "3_";
    case "constant":
      return "4_";
    default:
      return "9_";
  }
}

function parseImports(text) {
  const imports = new Map();
  let match;

  while ((match = MODULE_IMPORT_RE.exec(text)) !== null) {
    const modulePath = match[1];
    if (!modulePath.startsWith("jik/")) {
      continue;
    }

    const moduleName = modulePath.split("/").pop();
    const alias = match[2] || moduleName;
    imports.set(alias, moduleName);
  }

  return imports;
}

function resolveQualifiedSymbolAtPosition(document, position) {
  const lineText = document.lineAt(position.line).text;
  const imports = parseImports(document.getText());
  const index = getStdlibIndex();
  const qualifiedNameRe = /\b([A-Za-z_][A-Za-z0-9_]*)::([A-Za-z_][A-Za-z0-9_]*)\b/g;
  let match;

  while ((match = qualifiedNameRe.exec(lineText)) !== null) {
    const alias = match[1];
    const symbolName = match[2];
    const aliasStart = match.index;
    const symbolStart = aliasStart + alias.length + 2;
    const symbolEnd = symbolStart + symbolName.length;

    if (position.character < symbolStart || position.character > symbolEnd) {
      continue;
    }

    const moduleName = imports.get(alias);
    if (!moduleName) {
      return undefined;
    }

    const moduleInfo = index[moduleName];
    if (!moduleInfo || !Array.isArray(moduleInfo.symbols)) {
      return undefined;
    }

    const symbol = moduleInfo.symbols.find((item) => item.name === symbolName);
    if (!symbol) {
      return undefined;
    }

    return {
      moduleName,
      symbol,
      range: new vscode.Range(
        new vscode.Position(position.line, symbolStart),
        new vscode.Position(position.line, symbolEnd)
      )
    };
  }

  return undefined;
}

function getStdlibIndex() {
  if (liveIndexCache) {
    return liveIndexCache;
  }

  const stdlibPath = resolveStdlibPath();
  if (!stdlibPath) {
    return bundledIndex;
  }

  try {
    liveIndexCache = buildIndexFromDirectory(stdlibPath);
    if (Object.keys(liveIndexCache).length > 0) {
      return liveIndexCache;
    }
  } catch {
    liveIndexCache = null;
  }

  return bundledIndex;
}

function resolveStdlibPath() {
  const configured = vscode.workspace.getConfiguration("jik").get("stdlibPath");
  const candidates = [];

  if (configured && typeof configured === "string" && configured.trim() !== "") {
    candidates.push(configured.trim());
  }

  for (const folder of vscode.workspace.workspaceFolders || []) {
    candidates.push(path.join(folder.uri.fsPath, "jiklib"));
  }

  candidates.push("/opt/jik/jiklib");

  for (const candidate of candidates) {
    if (isStdlibDirectory(candidate)) {
      return candidate;
    }
  }

  return null;
}

function isStdlibDirectory(dirPath) {
  try {
    return fs.statSync(dirPath).isDirectory() && fs.statSync(path.join(dirPath, "std.jik")).isFile();
  } catch {
    return false;
  }
}

function loadBundledIndex() {
  return JSON.parse(fs.readFileSync(BUNDLED_INDEX_PATH, "utf8"));
}

function buildIndexFromDirectory(dirPath) {
  const index = {};
  const files = fs.readdirSync(dirPath, { withFileTypes: true });

  for (const entry of files) {
    if (!entry.isFile() || !entry.name.endsWith(".jik")) {
      continue;
    }

    const filePath = path.join(dirPath, entry.name);
    const moduleName = path.basename(entry.name, ".jik");
    index[moduleName] = parseStdlibModule(moduleName, fs.readFileSync(filePath, "utf8"));
  }

  return index;
}

function parseStdlibModule(moduleName, source) {
  const lines = source.split(/\r?\n/);
  const symbols = [];
  const moduleDocLines = [];
  let pendingDoc = [];

  for (let i = 0; i < lines.length; i++) {
    const rawLine = lines[i];
    const line = rawLine.trim();

    if (line === "@embed{C_END}") {
      break;
    }

    if (line.startsWith("//!")) {
      if (symbols.length === 0) {
        moduleDocLines.push(cleanDocLine(line.slice(3)));
      }
      continue;
    }

    if (line.startsWith("///")) {
      pendingDoc.push(cleanDocLine(line.slice(3)));
      continue;
    }

    if (line === "") {
      if (pendingDoc.length > 0) {
        pendingDoc.push("");
      }
      continue;
    }

    if (/^\s/.test(rawLine)) {
      continue;
    }

    let symbol = null;

    if (/^extern\s+struct\b/.test(line) && /\bas\s*$/.test(line)) {
      const nameLine = nextNonEmptyLine(lines, i + 1);
      if (nameLine) {
        symbol = {
          name: nameLine.trim(),
          kind: "struct",
          signature: nameLine.trim(),
          doc: normalizeDoc(pendingDoc)
        };
      }
    } else if (/^extern\s+(throws\s+)?func\b/.test(line) && /\bas\s*$/.test(line)) {
      const signatureLine = nextNonEmptyLine(lines, i + 1);
      if (signatureLine) {
        symbol = parseFunctionSignature(signatureLine.trim(), pendingDoc, /throws/.test(line));
      }
    } else if (/^(throws\s+)?func\b/.test(line)) {
      symbol = parseFunctionSignature(trimFunctionBodyMarker(line), pendingDoc, /^throws\b/.test(line));
    } else if (/^struct\s+/.test(line)) {
      const name = line.replace(/^struct\s+/, "").replace(/:$/, "").trim();
      symbol = {
        name,
        kind: "struct",
        signature: name,
        doc: normalizeDoc(pendingDoc)
      };
    } else if (/^enum\s+/.test(line)) {
      const name = line.replace(/^enum\s+/, "").replace(/:$/, "").trim();
      symbol = {
        name,
        kind: "enum",
        signature: name,
        doc: normalizeDoc(pendingDoc)
      };
    } else if (/^[A-Za-z_][A-Za-z0-9_]*\s*:=/.test(line)) {
      const name = line.split(":=")[0].trim();
      symbol = {
        name,
        kind: "constant",
        signature: name,
        doc: normalizeDoc(pendingDoc)
      };
    }

    if (symbol) {
      symbols.push(symbol);
      pendingDoc = [];
    } else {
      pendingDoc = [];
    }
  }

  return {
    module: moduleName,
    doc: normalizeDoc(moduleDocLines),
    symbols: symbols.filter(Boolean)
  };
}

function parseFunctionSignature(signatureLine, pendingDoc, throwsFlag) {
  const sanitized = sanitizeSignature(signatureLine);
  const nameMatch = sanitized.match(/^([A-Za-z_][A-Za-z0-9_]*)\s*\(/);
  if (!nameMatch) {
    return null;
  }

  return {
    name: nameMatch[1],
    kind: "function",
    signature: sanitized,
    doc: normalizeDoc(pendingDoc),
    throws: throwsFlag
  };
}

function nextNonEmptyLine(lines, start) {
  for (let i = start; i < lines.length; i++) {
    const trimmed = lines[i].trim();
    if (trimmed !== "") {
      return lines[i];
    }
  }
  return null;
}

function trimFunctionBodyMarker(line) {
  return line.endsWith(":") ? line.slice(0, -1).trim() : line.trim();
}

function sanitizeSignature(signature) {
  return signature
    .replace(/^(?:throws\s+)?func\s+/, "")
    .replace(/\bforeign\s+/g, "")
    .replace(/\s+/g, " ")
    .trim();
}

function cleanDocLine(text) {
  return text.replace(/^\s*/, "").trimEnd();
}

function normalizeDoc(lines) {
  if (!Array.isArray(lines)) {
    return "";
  }

  const filtered = [];
  for (const line of lines) {
    if (line === "") {
      if (filtered.length > 0 && filtered[filtered.length - 1] !== "") {
        filtered.push("");
      }
      continue;
    }
    if (line.startsWith("@")) {
      continue;
    }
    filtered.push(line);
  }

  while (filtered.length > 0 && filtered[filtered.length - 1] === "") {
    filtered.pop();
  }

  return filtered.join("\n").trim();
}

function makeSymbolMarkdown(moduleName, symbol) {
  const signature = symbol.signature || symbol.name;
  const markdown = new vscode.MarkdownString(undefined, true);
  markdown.appendCodeblock(moduleName + "::" + signature, "jik");
  if (symbol.doc) {
    markdown.appendMarkdown("\n\n" + escapeMarkdown(symbol.doc));
  }
  if (symbol.throws) {
    markdown.appendMarkdown("\n\nThrows on failure.");
  }
  return markdown;
}

function escapeMarkdown(text) {
  return text.replace(/[\\`*_{}[\]()#+\-.!]/g, "\\$&");
}

module.exports = {
  activate,
  deactivate
};
