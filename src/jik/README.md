# Jik compiler

This is the primary compiler, written in Jik. Normal builds use the bundled
C seed in `src/seed/` to compile these sources.

Run these commands from the repository root:

```sh
make                      # Build the current compiler as jik
make test                 # Build and test the current compiler
make test-bootstrap-full  # Test the seed and three self-hosted stages
```

## Reading the compiler

Start with [compiler.jik](compiler.jik), which runs one compilation, then
[semantic/semantic.jik](semantic/semantic.jik), which runs the analysis phases.
The compiler stores types and region information on the AST and generates C
directly from that checked tree. The CLI then invokes the C compiler.

## Pipeline and what each step provides

Loading finds imports while lexing and parsing each source file. Each source
and module gets an ID that stays fixed. Separate lists of those IDs determine
analysis and initialization order; the module collection stays in discovery order.
Each analysis phase runs only if the previous phase succeeded. A failed phase
can leave incomplete data, so C generation starts only after all reachable
modules pass analysis.

| Phase and entry point | Required input | Successful result |
| --- | --- | --- |
| Load: `modules/module_loader::load_modules` | Entry path and configured source/import roots | Located tokens, parsed module ASTs, resolved import edges, stable source IDs. Source I/O throws; lexical, parse, or import diagnostics also stop loading. |
| Graph: `modules/module_graph::dependency_order`, then `initialization_order` | Complete loaded modules and valid import edges | Dependency order after checking for import cycles, plus the separate module initialization order. Both are stored in Program. |
| Resolve: `semantic/resolution/resolve::resolve_declarations` | Parsed modules in dependency order | Shared type store, declarations, scopes, resolved annotations/references, table metadata, and prepared global initializers. Receiver lookup can wait for inference. |
| Entry: `resolve::check_entry` | Resolved declarations and the actual entry source ID | A valid main declaration. Type inference checks that it returns int and, if it takes an argument, accepts Vec[String]. |
| Infer: `semantic/typing/inference::run` | Resolved declarations, annotations, and the shared store | Types on values/bindings and complete function signatures, stored only after validation of all modules. Temporary slots are cleared on success and failure. |
| Check: `semantic/typing/typecheck::run` | Stored types and references | Valid operand/result, condition, assignment, call, and error-handling rules. Its direct API also diagnoses inconsistent or missing metadata. |
| Regions: `semantic/regions/regions::run` | Successfully type-checked syntax | Function-safety flags, region sources, allocation destinations, local-region requirements, and validated stores/returns/literals. |
| Emit: `codegen/codegen::generate` | Successful semantic analysis and both prepared schedules | One C translation in the caller-selected output region. Checked targets/types/destinations drive emission. |
| Native: `cli/native_build::compile` | Compiler configuration, C tool, and native requirements | C is written, the target profile is validated, the host compiler runs, and requested assets are copied. Preview stops before host compilation and copying. |

Semantic entry points record diagnostics rather than throwing source errors.
Loading, driver, output I/O, and native-build functions pass on recoverable
failures. The driver marks printed diagnostics with `ERROR_REPORTED` so CLI error
handling can avoid printing them again. Code that calls a phase directly must
prepare its required inputs. Having no errors does not mean earlier phases have
run or filled in the data that phase needs.

The compiler driver uses `compiler::verbose` for optional stage messages. Lexing
and parsing are reported once for all sources. The semantic entry point receives
a verbose flag and reports program analysis and region checking to stderr without
depending on the driver or CLI modules. No per-module progress messages are printed.

## Finding a rule

| Area | Starting modules |
| --- | --- |
| Commands and native integration | `main.jik`, `cli/cli.jik`, `cli/commands/`, `cli/native_build.jik` and `native_profiles.jik` |
| Source identity, imports, diagnostics | `source.jik`, `diag.jik`, `modules/import_path.jik`, `module_loader.jik`, and `module_graph.jik` |
| Tokens and grammar | `lexer/token.jik`, `lexer/lexer.jik`, `parser/state.jik`, `expressions.jik`, `statements.jik`, and `parser.jik` |
| Persistent definitions | `program.jik`, `modules/module.jik`, `parser/ast.jik`, and `language/` |
| Names and declarations | `semantic/resolution/resolve.jik`, `bindings.jik`, `lookup.jik`, `calls.jik`, and `type_resolution.jik` |
| Type inference and validation | `semantic/typing/solver.jik`, `constraints.jik`, `inference.jik`, `typecheck.jik`, and shared `rules.jik` |
| Lifetime and initialization rules | `semantic/regions/` and `semantic/initialization/` |
| C names, types, helpers, and code generation | `codegen/c_names.jik`, `backend.jik`, `type_support.jik`, `operations.jik`, and `codegen.jik` |

Command implementations live in `cli/commands/`. Simple commands use one module
each; `cli/commands/test/` contains the test command, discovery and runner
generation. Filesystem link inspection is provided by `jik/fs`. CLI dispatch,
command metadata, and shared build and argument utilities remain directly
under `cli/`.

For a syntax change, start with its token and parser function, then the AST payload
and the resolution/type/region cases that consume it. For a new builtin, start
with `language/builtin.jik`, its shared type rules, and its checking/emission
cases. Keep acceptance and lifetime decisions in semantic analysis; the backend
chooses the C form from checked data. Existing tests call individual phases as
well as compiled programs, so inspect both when changing those requirements.

## Shared program data and semantic contexts

`program.jik` defines the data kept for one compilation: sources, modules,
analysis and initialization order, diagnostics, and the shared type store. It
imports data definitions rather than the code that runs the compiler phases.
`compiler.jik` combines this `Program` with command options and runs loading,
analysis, C generation, and file output.

Modules and AST nodes retain stable integer IDs. `Program.modules` is indexed by
source ID; `Program.dependency_order` describes traversal without rearranging storage.
`Program.initialization_order` stores the order used for startup and native build requirements.

Declaration resolution initializes `Program.type_store` and attaches the same
value to each `Module.type_store`. Module helpers and code generation use this
shared store through that field. Type IDs refer to it throughout the program.
A failed phase can leave incomplete data, so later phases must stop if an earlier
phase reported errors.

`semantic/context.jik` selects a program and one of its modules for a recursive
semantic walk. Resolution, type checking, and region analysis pass this
`ModuleContext` rather than repeatedly passing modules, the current module, and
diagnostics. Recursion keeps its module selection; node IDs, lexical scope,
expression/error modes, and function-specific inputs remain explicit where they
change the operation. Module-local helpers can still take just the module or
type store they need.

The program, module views, and module contexts share the caller's compilation
region. Contexts do not copy module collections or type stores. Inference receives
the program at its phase entry points, but its `Solver` and union-find slots remain
temporary analysis data. It stores types only after validation and clears
temporary node slots on both success and failure.

Phase-level tests use the same `Program` as production code. A test that exercises
only parsed or resolved modules need not configure the command line or populate
source-manager fields that its phase does not read. Tests that call entry checking
or code generation must supply the source information those phases require.
Direct code-generation callers also supply both prepared schedules.

## Parser and inference modules

Parsing follows a one-way dependency chain. `parser/state.jik` owns the token
cursor, error recovery points, and type-name grammar. `parser/expressions.jik`
owns literals, postfix operations, and the complete precedence chain.
`parser/statements.jik` combines expressions into local declarations, assignments,
and nested bodies. `parser/parser.jik` parses module declarations and provides the
`parse` entry point. These modules share one Parser and AST. Token operations
use UFC in the Parser's defining module; grammar calls across modules are qualified.
Parser error paths restore literal depth and resume at statement or declaration
boundaries.

UFC is useful for operations on a value whose defining module supplies them:
`p.advance()`, `g.output.write(...)`, and `arguments.push(...)` already follow
that convention. Imported struct UFC resolves in the type's defining module;
importing a grammar helper does not add it as a method of Parser. Grammar calls
such as `expressions::parse_expression(p)` therefore remain qualified. Semantic
phase calls and cross-module relationships also retain qualification where it
identifies the responsible phase more clearly than the receiver alone.

Inference separates three responsibilities. `semantic/typing/solver.jik` owns
temporary slots, union-find operations, and first-error reporting.
`semantic/typing/constraints.jik` initializes slots from annotations and entry
requirements, then collects constraints from the resolved tree.
`semantic/typing/inference.jik` repeats solving until no types change (a fixed
point), resolves receiver calls once their types are known, checks and saves
inferred types, and clears temporary slots on success or failure.
Resolving a receiver call can add a region argument, so inference collects that
call's constraints again. Both modules use the solver without importing each
other. The Solver is temporary and stores no module or diagnostic references.
Final types belong to the shared compilation store.

Resolution builds scopes and resolves expressions and blocks. Separate modules
handle type registration, omitted region arguments, lookup, bindings, and
initializer rules. Type checking keeps the expression, builtin, match, and block
checks together because they call each other recursively. Region analysis does
the same for choosing allocation regions and checking stores, returns, and calls;
function-safety inference and literal rules have their own modules.

## Where checks belong

When inference succeeds, AST nodes have their final types.
The type checker can also be called directly on an AST whose types or references
have been changed. Tests deliberately remove types, change operands, and corrupt
call targets. The checker must report these errors rather than assume every
caller has just run inference on an unchanged tree.
Call checking validates a target's module index, declaration index, and function
kind before reading its signature; later signature validation relies on that
same-call guarantee rather than repeating the function-kind check.

Each phase checks for earlier errors before starting. Recursive checks also stop
when a child reports an error, so they remain necessary after the initial check.
Inference saves types only if it succeeds, and clears temporary slots either way.
The compiler refuses to generate C after failed analysis. Direct callers of code
generation must provide checked data and both module-order lists.

## Module order and C generation order

`compiler::analyze` loads modules and obtains `module_graph::dependency_order`,
which validates the import graph without rearranging module storage. After its
graph check succeeds, the compiler computes `Program.initialization_order`
using the entry source ID. Both lists belong to the compilation region and
contain stable source IDs. Analysis uses dependency order; C generation uses
both lists. An import-cycle error stops compilation before initialization order
is computed.

| Order | Where it is built and used | Required behavior |
| --- | --- | --- |
| Stable source IDs | Module loader; all indexed storage and resolved references | Modules stay at their discovery IDs, independently of either schedule. |
| Dependency traversal | DFS in `module_graph::dependency_order`; AST dumps, semantic passes, profile validation, C declaration scans | Visit imports in written order, append dependencies before importers, retain diagnostic traversal and import-cycle reporting. |
| Startup and native build order | `module_graph::initialization_order`, stored by the compiler; global C initializers and native build requirements | Match the seed's ordering of sibling modules, include/link searches, archives, and file-copy overwrites. |
| Table initialization | Declaration resolution using dependency traversal and written declaration order; table reference validation and C initialization | Earlier tables are available to later tables; all tables are initialized before ordinary globals. This is distinct from the global startup schedule. |

The startup algorithm uses a last-in, first-out worklist to find reachable
modules. It adds modules with no imports in reverse discovery order, then
repeatedly scans the remaining modules in discovery order and adds those whose
imports are ready. Both module orders put imports before importers, but sibling
modules can appear in different orders. Swapping the lists could change global
side effects, linker search results, or which copied file overwrites another.
Native build arguments and file copying use the saved initialization order.
Within a module, directives and global declarations stay in source order.

C emission has additional constraints. A single translation unit still requires
declarations and helper definitions to precede their uses:

| Emission phase | Reason for its position and retained order |
| --- | --- |
| Runtime preamble, then embedded C | Runtime definitions and embedded headers/types/prototypes must be available to generated declarations. Embedded blocks retain dependency traversal and source declaration order; arbitrary user C is not reordered or analyzed. |
| Enums | Definitions and enum string helpers precede data layouts and printing. Shared type-store order gives deterministic output. |
| Record/container support | Collection marks visited types before recursion. Forward tags permit recursive record/container pointers; record layouts and constructors precede option/vector/dictionary helpers. Nested vectors are collected before containing vectors; dictionary lookup options precede dictionary helpers. The category order and recursion guards remain necessary. |
| Copy and printing helpers | All layouts/constructors exist first. Copy helpers are deduplicated by type. Printer collection retains dependency order and emits every printer prototype before definitions, allowing recursive types. |
| Table and global storage | Types already exist; storage declarations precede every user function that may access them. Table and global declaration scans retain dependency traversal for deterministic output. |
| Function and must-wrapper prototypes, then bodies | Prototypes permit forward and mutually recursive calls, including throwing wrappers. External normal prototypes belong to the embedded C/header so C's permitted conversions remain intact. |
| C main | Instrumentation/global-region setup precedes table initialization, then ordinary global initialization, argument construction, the Jik entry call, and cleanup. Table order and global side-effect order have separate ordering rules above. |

Visited-type checks prevent infinite recursion while collecting recursive types.
Forward declarations and prototypes allow generated types and functions to refer
to one another. Consistent declaration order also makes generated C easier to
inspect. Function emission separately handles expression evaluation and cleanup.

## Backend modules and state

`codegen::generate` is the compiler's entry point for C generation. It accepts
`Options` and returns generated text. `codegen.jik` imports type support and
operations; both use helpers in `backend.jik`, which imports C-name and writer
modules. These dependencies run in one direction to avoid import cycles.

`version.jik` supplies the shared release identity for CLI version/environment
reporting and the generated C version comment. The comment precedes the runtime
preamble in both included-header and embedded-runtime output, matching the seed.

| Module | Responsibility |
| --- | --- |
| `codegen/codegen.jik` | Overall C generation, table/global storage, and C main startup/cleanup. Its emission sequence retains the ordering rules above. |
| `codegen/backend.jik` | `Translation`, C names and types, string escaping, source locations, and region instrumentation calls. |
| `codegen/type_support.jik` | Enum support, recursive data collection/layouts, collection constructors/helpers, copy helpers, and printing helpers/prototypes. `emit_data_types` includes records, Options, vectors, and dictionaries. |
| `codegen/operations.jik` | Generate C directly from checked syntax: expressions, statements, function bodies, signatures, must wrappers, and cleanup. |
| `codegen/c_names.jik`, `codegen/writer.jik` | Existing deterministic type-name fragments and buffered line/indentation control. |

`backend::Translation` contains the shared writer, source/module views, prepared
integer schedules, translation settings, global-region requirement, and type/helper
collections and lists of visited types. These collections grow throughout C
generation and are shared by function contexts. Helpers for writing names and
types use the checked AST. Type support writes layouts and helpers; operation
emission handles evaluation order and cleanup.

`operations::BodyContext` contains a reference to that translation and the current
function's local-region requirement, result type, and throwing convention.
`emit_function` constructs a context from the checked function/signature and passes
it through recursive operations. A local `g` is an alias of `body.translation`,
not a second writer or program state. The signature and must-wrapper emitters need
only translation state. Startup gets a separate context with no local region or
throwing convention; its unused result-type field is never read by an expression.
It cannot inherit the last emitted function's cleanup state.

Error destinations remain explicit inputs to call emission. Checked AST handler
IDs select nested error-message/code references; propagation uses the current
function's error argument. Temporary names use node IDs or nested C scopes
(for example, compound-assignment captures). Return and cleanup decisions use
the function context; startup uses the translation's global-region flag.
`operations.jik` handles evaluation order, bindings created only on success,
error-message lifetimes, and cleanup order.

Translation and body contexts share the caller-selected compilation/output region,
as do the module views. Contexts are small references/state records allocated in
that region; no AST, module list, or type store is copied, and no compiler storage
is placed in the compiled program's global region. The module-order lists are
copied as integer vectors into the output region. Recursive expression and
statement emitters share a module because they call one another.

### Writing C types

`c_names::CTypeNames`, keyed by `types::Atom`, is the sole backend mapping from
atomic Jik types to base C names. Container identifiers use these base names
without pointer or declaration syntax. `c_names::write_atom_type` writes a
complete C type, adding the pointer for String and Region in one place.
`backend::write_type` delegates its atomic case to this writer; named and
container types retain their existing shared naming functions.

Generated temporary declarations, casts, runtime-region fields/variables,
String-array literals, and copy/printing helper signatures also use this mapping.
The foreach index expression uses the table's scalar INT name when building its
cast text. Use this mapping rather than hardcoding a second name for a mapped
type. Return types and container names must use the same base name, although
their pointer and declaration syntax differs.

Native C interface types remain explicit where they have a different purpose:
`int`/`char **` for C main, `size_t` for runtime indexing, `char *` for native
diagnostic text, and `void` for C parameter-list markers/opaque pointers. Internal
runtime support types such as JikError and JikCharBuffer are not Jik atomic types.
Runtime function/macro symbols are separate ABI names, not derived from a C type
name. The runtime header defines that ABI.

## AST walks and mutation

Use root declaration lists for declaration passes and block statement lists for
scope/control-flow walks. Dispatch on `NodeKind` with `match` payload bindings
when several alternatives are handled. Single-kind guards and direct integer
indexing remain appropriate; the compiler does not need a generic visitor or a
getter for each AST field.

Scope collection registers a block's locals before setting up child scopes. It
walks statements twice without changing the tree. Expression resolution follows
operand lists and can rewrite a node while retaining its ID and span. Omitted
Region arguments are appended before argument traversal; default struct fields
are typed leaf nodes appended after written fields are resolved. Default variant
payloads are appended before the payload walk visits them.

Inference deliberately scans all node storage in ID order on each fixed-point
sweep. Iteration reads the vector's current length, including Region arguments
inserted as receiver calls are resolved. The new arguments receive constraints
immediately. Result checking, type saving, and slot cleanup also include inserted
nodes; cleanup must run after failure too.
Region analysis changes metadata without adding nodes; final literal-region checks
scan storage after all retargeting, preserving their diagnostic order.

Semantic operation names describe their work: `collect_block_scopes`,
`check_block`, `analyze_expression_regions`, and `expression_needs_local_region`.
Type checking uses `check_value` where an expression must produce a value.
`check_expression` also handles statements and the rules for try/must calls,
including where a void result is allowed.

## Region analysis

`language/region_source.jik` describes regions in the compiled program. This is
separate from the compilation region that stores the AST and contexts. Each
`Node.region_source` records the region kind and, where needed, the parameter
that supplies it. It does not contain a runtime region ID.

| Class | Meaning |
| --- | --- |
| NONE | No region source has been assigned or is needed. Many scalar/syntax nodes retain this state. |
| LOCAL | The current function's local region. |
| GLOBAL | Storage that lasts for the compiled program's lifetime. |
| CALLER | A non-foreign composite or Region parameter supplies the allocation region. |
| FOREIGN | A foreign composite parameter supplies an independent input region, which cannot supply a shared result. |

`Source.parameter_id` identifies the parameter for CALLER/FOREIGN. `allocation_target`
identifies an explicit destination expression or selected parameter; absent a
target, the emitter uses the checked LOCAL/GLOBAL class. A Region operand can
carry a region source even though the operand itself is not an allocated composite.
`same_region` compares source classes under the shared-call rules, excluding
NONE/FOREIGN. CALLER parameters can have different origins but share a region
under a storing/returning function's call rules. Function-safety inference
instead tracks each input separately when checking whether a function is safe
to call with arguments from different regions.

The pass first repeats function-safety checks until no flags change (a fixed
point), using the stored types and resolved calls. It then visits modules in
dependency order: reset previous region results and literal exceptions, assign
table regions, check global initializers, and check functions. Function analysis assigns parameter sources,
walks expressions/stores/returns, and determines local-region cleanup requirements
after eligible literals have been retargeted. A final storage scan checks nested
literal regions after all calls/stores/returns have completed their retargeting.

`AnalysisScope` distinguishes GLOBAL_INITIALIZER from FUNCTION with its actual
declaration. Recursive expression analysis carries this scope without a dummy
function or a separate global-mode boolean. The block walk is entered only with
FUNCTION and retains that scope through nested statements. Both scope variants
are allocated in the compilation region, not the global region of the compiled
program.

`literal_rules` checks which literals can have their allocation region chosen
automatically. A fresh composite tree contains newly created composite literals;
scalar expressions are allowed, and dictionary keys are copied. Function-safety
analysis checks this structure before regions have been assigned. Region checking
then validates the chosen regions. These walks answer different questions. The shared `is_region_input` check covers Region operands and
allocated composites in both parameter setup and shared-call validation.

`Node.foreign_argument_literal` marks only an eligible fresh local Vec/Dict literal
passed directly to a foreign parameter, allowing grouping around that literal.
Its final entry-region check is skipped; nested literals are checked separately.
The emitter does not read this flag: ordinary constructors allocate local container
storage, copy vector elements or dictionary values as values/references, and use
the usual dictionary-key copying. Composite entries are not deep-copied or moved
to another region. The container lasts until the current function's local region
is destroyed; there is no special release at the end of the call. The foreign
parameter's existing restrictions prevent retaining or returning those references
beyond the call. Explicitly allocated literals and
already-bound containers do not acquire this exception.

## Struct documentation

Struct declarations should be understandable without searching for the code that
creates or reads them. Use ordinary `//` comments and the following conventions.

- Put a short purpose comment immediately above each struct. Explain its role
  and relevant scope: one module, one compilation, one function, or a temporary
  analysis. Put the purpose before longer explanations of rules for its fields.
- Describe ambiguous fields beside their declarations. For integer references,
  identify the target collection and its scope: AST node ID in the containing
  module, source ID in `SourceManager.sources`, type ID in the shared
  `types::Store.types`, or slot ID in `Solver.slots`. Apply the same rule to
  optional references and collections of references.
- Distinguish IDs from positions, counts, and source offsets. State index bases,
  byte units, and inclusive/exclusive bounds where applicable.
- For data filled in during analysis, explain when it becomes valid and what
  `None`, a special marker such as `-1`, or an unused field means. Name the producing phase when useful;
  avoid repeating the complete pass sequence on every field.
- Explain ordering, parallel collections, and conditional relationships above
  the affected field group. Document shared stores and unusual cross-region references or
  lifetime constraints, without repeating ordinary Jik region rules.
- Leave self-explanatory fields uncommented. Prefer a clearer name when a comment
  would only explain an unclear name. Update callers and tests when renaming.
- Keep short comments inline, separated from the declaration by four spaces.
  Move longer explanations above the field and wrap prose at about 100 columns.
  Use consistent domain terms rather than mandatory documentation tags.

Module-level conventions can avoid repetition. In `parser/ast.jik`, unqualified
node IDs refer to the containing module's `Ast.nodes`, while type IDs refer to
the compilation's shared type store. Field comments still identify a node's role
(for example, subscript expression or type annotation). Cross-module references
state how to select the module as well as the node.

For example:

```jik
// Union-find entry for a temporary type-inference variable.
struct Slot:
    parent: int    // Index in Solver.slots; a root points to itself.
    rank: int    // Union-by-rank heuristic, not the current tree depth.
    bound: Option[int]    // Type ID in the shared compilation Store; None when unbound.
end
```

Verify documentation against the code that writes and reads the data. Preserve
explanations of important rules when improving existing comments. Treat suspected behavior
problems separately from wording changes.
