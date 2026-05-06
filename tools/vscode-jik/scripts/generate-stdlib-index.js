"use strict";

const fs = require("fs");
const path = require("path");

const repoRoot = path.resolve(__dirname, "..", "..", "..");
const stdlibDir = path.join(repoRoot, "jiklib");
const outPath = path.join(__dirname, "..", "data", "stdlib-index.json");

function main() {
  const index = {};
  const entries = fs.readdirSync(stdlibDir, { withFileTypes: true });

  for (const entry of entries) {
    if (!entry.isFile() || !entry.name.endsWith(".jik")) {
      continue;
    }

    const moduleName = path.basename(entry.name, ".jik");
    const filePath = path.join(stdlibDir, entry.name);
    index[moduleName] = parseStdlibModule(moduleName, fs.readFileSync(filePath, "utf8"));
  }

  fs.writeFileSync(outPath, JSON.stringify(index, null, 2) + "\n", "utf8");
  process.stdout.write("Wrote " + outPath + "\n");
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
  if (nameMatch[1].startsWith("_")) {
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

main();
