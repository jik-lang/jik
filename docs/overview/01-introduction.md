[Back to overview](../overview.md)

# 1. Introduction

Jik is a statically typed compiled language with region-based memory management, targeting C.
It is a compact language which is meant to be easy to learn and easy to use.

Jik is designed around explicit structure, predictable compilation, and a small language surface.
Programs are translated to C, which keeps the toolchain simple and makes the generated output easy
to inspect, build, and debug with existing C tooling.

This overview is a guided tour of the language rather than a formal specification. Its purpose is
to show how Jik code is written, what the core features look like in practice, and how the main
parts of the language fit together.

---
