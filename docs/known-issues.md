# Known Issues and Current Limitations

This document lists implementation limitations and known technical issues in the current Jik version.


## Current status

Jik is currently in alpha. The language is stabilizing, but some implementation details are still
being hardened and some portability work is intentionally deferred.

## Known limitations

- Generated C is currently intended for `clang` and `gcc`. MSVC is not part of the default supported path.
- Some generated code currently relies on GNU-compatible C extensions, especially for vector
  repeat-initializers of the form `[n of expr]`.
- Portability work for strict non-GNU C toolchains is not complete.
- Some compiler command paths still construct shell command strings for host compiler and tool
  execution. This is a known robustness and portability limitation, especially for unusual paths,
  quoting edge cases, or host tool configurations.

## Known runtime issues under review

- Region allocator arithmetic is not fully hardened against extreme allocation-size overflow yet.
- Region allocator alignment is not yet guaranteed to match alignof(max_align_t) on all host ABIs, especially for some FFI-defined C types.

### Scope note

This page should stay short and practical. It is meant to document known user-visible limitations and
important implementation issues, not to replace internal task planning documents.
