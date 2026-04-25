# Jik Language Tools

This extension provides:

- syntax highlighting for `.jik` files
- import-aware stdlib member completion after `module::`
- hover documentation and signatures for imported `jiklib` members
- bundled `jiklib` metadata so completions work outside the Jik repository

## Completion behavior

The completion provider inspects `use` directives in the current file.

Examples:

```jik
use "jik/io"
use "jik/testing" as test

func main():
    io::
    test::
end
```

Only imported `jik/...` modules are considered. If an alias is used, the alias must be used at the call site.

## Stdlib source

The extension uses a bundled stdlib index generated from the repository `jiklib/` directory. If you want completions to reflect a different local Jik installation, set:

```json
"jik.stdlibPath": "/path/to/jiklib"
```

When opened inside the Jik repository, the extension automatically uses the workspace `jiklib/` directory. In the `try-jik` devcontainer it also detects `/opt/jik/jiklib`.

## Packaging

Generate the bundled index:

```bat
node scripts\generate-stdlib-index.js
```

Package the extension:

```bat
vsce package
```
