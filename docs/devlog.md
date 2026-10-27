# LODE development log

A running record of progress, design decisions and their reasons, kept for review. The
**Status** and **Decisions** sections are kept current; the **Log** grows at the bottom, one
entry per stretch of work. Commits are named by their short hash.

## Status

| Phase | Scope | State | Commit |
| --- | --- | --- | --- |
| 0 | Scaffold: CMake, header-only target, test harness, license | Done | `e4ae3d2` |
| 1 | Expression core: numbers, symbols, sums, products, powers in canonical form | Done | `a55ce30` |
| 2 | Differentiation, substitution, `exp` `log` `sin` `cos`, reference evaluator | Done | `0838c4e` |
| 3 | `Function`: compiled fast evaluation, batches, gradient/Jacobian/Hessian, partial evaluation | Done | `0e09903` |
| 3b | Parameters: inputs excluded from derivatives | Done | `743e2e2` |
| 4 | Examples of usage and core features; chemical kinetics application | Done | `83b8218` |
| 5a | C++ code generation | Done | `be59a87` |
| 5b | `expand`, `collect` | Planned | |
| — | Discussion: indexing (R6) | Done: convention settled (D27) | |
| — | Published on GitHub, with continuous integration | Done (D28) | |
| — | Discussion: tensor-valued domains and codomains of functions | Done: decided (D30) | |
| 6 | Tensor container; functions indexed through `outputs()`; tensor-valued domains | Done | `037e94b` |
| 7 | Sparse Jacobians, with MATL interoperability | Done | *Add sparse Jacobians* |
| — | Arbitrary-precision exact numbers (agreed, D33) | Later, before high-order uses | |
| — | A tensor container wrapping contiguous results, of any rank | Planned | |

## Requirements from you

- **R1. Fast evaluation.** Evaluating a function (and its derivatives) must be as fast as
  possible. Building and simplifying expressions may be slower and more careful.
- **R2. Partial evaluation.** For `f(x, y, z)`, `f(x, 2, z)` should give a new function
  `g(x, z)` that can be kept and evaluated faster than `f`.
- **R3. Use cases.** Shape functions for finite elements; systems of ODEs that need exact
  Jacobians, such as reactive chemistry.
- **R4. Style.** Your C++ style guide (C++20, `this->`, `east const`, title blocks, rules ending at
  column 100, `-Wall -Wextra -Werror` on GCC and Clang).
- **R5. License.** BSD 3-Clause.
- **R6. Indexing reflects the object.** Mathematical objects (matrices, tensors) are indexed
  from 1, as in the mathematics; arrays in the computer-science sense from 0, as offsets from an
  address. MATL follows this. For LODE: see D27.
- **R7. C++20 at most.** LODE should build with C++20 compilers, because some HPC systems lag
  behind the latest standards. Newer features (C++23 and later) can be revisited later; low
  priority.

## Decisions

Each decision records what was chosen and why. Later entries may revise earlier ones; revisions
are noted in place.

**D1. Start a new repository rather than evolve nablaxx.** The prototype's representation
(binary expression trees with pairwise rules) was the cause of the simplification problems, so
the core had to change anyway. nablaxx stays untouched for reference.

**D2. Name: LODE**, *Library for Operators, Derivatives and Expressions*. Chosen from your
shortlist (CEDAR, SODA, LODE, SPADE) after checking for clashes: LODE had none in math or
scientific computing; SPADE had a small C++ differential-equation library and a Rust mesher;
CEDAR collides with AWS Cedar and a Canadian supercomputer; SODA is close to an existing
symbolic differential algebra package (SDA).

**D3. Header-only C++20, GCC and Clang.** Uses the `__int128` extension for exact rational
arithmetic, which MSVC lacks. Tested with Apple Clang 21 and GCC 15.

**D4. Canonical forms instead of a simplification rule engine.** Every expression is built
directly in a unique normal form: sums are flat, sorted lists of coefficient × term plus a
constant; products are a coefficient times sorted base^exponent factors. Like terms and powers
combine as the expression is built. Equal expressions therefore have identical structure, and
`==` is a structural comparison. This is the approach of SymPy and SymEngine. Rule engines on
binary trees need a rule for every pairing of node types and nesting, and rules can undo each
other; nablaxx's factoring rule produced a wrong answer (`x*y + x` became `x*y + x^2`).

**D5. A closed set of node kinds in a `std::variant`, not a class hierarchy.** Kinds are few and
stable (number, symbol, sum, product, power, function call), while operations keep growing
(print, compare, differentiate, substitute, evaluate, compile). With a variant, each operation is
one function with a `switch`, readable in one place; adding a kind means updating each switch,
which the compiler flags. nablaxx already dispatched on `type_id()` and `static_cast` in its
simplifier, because simplifying depends on two operands' kinds at once.

**D6. Immutable, shared nodes.** `Expr` is a handle to a `shared_ptr<Node const>`. Copying is a
reference-count increment, subexpressions are shared rather than cloned (derivatives stay
small), and sharing is safe because nodes never change. Each node stores its hash at creation,
so unequal expressions are usually told apart in one comparison.

**D7. Exact numbers.** Coefficients are exact rationals with 64-bit parts; a result that would
overflow becomes a `double`. Floating-point constants (`2.5`) are a separate kind of number and
never equal an exact one (`2.0 != 2`), so exact and inexact values are not silently mixed.

**D8. Symbols have identity.** Each `Symbol` constructed is a new variable, even with a repeated
name; copies of it are the same variable. This avoids two unrelated `"x"` variables being
confused.

**D9. Products of sums are not expanded automatically.** `x*(y + z)` stays as written. Expansion
can blow up expressions, so it will be an explicit operation (phase 5).

**D10. A sum inside a product is kept primitive.** Its common numeric factor is taken out, with
the sign that makes its first term positive: `(2*x + 2*y)*z` is stored as `2*z*(x + y)`. Without
this, multiplying a number into a single sum (which distributes: `2*(x + y) = 2*x + 2*y`) made
grouping matter, and `2*(x + y)/(x + y)` did not simplify to `2`. Found by the randomized tests.

**D11. Only simplifications that hold for every real value are automatic.** For example,
`(x^2)^(1/2)` is `|x|`, so it is not reduced to `x`; `log(x^2)` is not rewritten as `2*log(x)`.
Rules that do hold are applied: `sqrt(x)^2 = x`, `exp(log(u)) = u`, `log(exp(u)) = u`,
`sin(-u) = -sin(u)`, `cos(-u) = cos(u)`, `exp(a)*exp(b) = exp(a + b)`.

**D12. Exponentials take logarithm terms out of their argument**:
`exp(c*log(u) + rest) = u^c*exp(rest)`. Without it, `exp(log(x))*exp(y) = x*exp(y)` differed from
`exp(log(x) + y)`, breaking the rule that exponentials combine. Found by the randomized tests.

**D13. The domain is not stored in the expression.** nablaxx wrote each variable's input index
into its node (`set_domain`). LODE binds variables to input positions only when a `Function` is
compiled. This keeps nodes shareable between functions with different domains and makes partial
evaluation (R2) a substitution followed by a recompile.

**D14. A reference evaluator, then a compiled one.** `evaluate` walks the expression and is
simple enough to trust; it is templated on the number type so that complex-step
differentiation can check `diff` to machine precision. The fast evaluator (phase 3) is checked
against it.

**D15. Testing strategy.** Besides specific cases, randomized tests build random expressions
and check that algebraic laws hold *structurally* (commutativity, associativity, inverses,
powers, exponentials), and that derivatives match complex-step values. Each randomized test is
also run over extra seeds before a phase is committed (300 for the algebraic laws, since the
first CI run showed that 20 missed rare cases). Every phase builds warning-free on Clang and GCC
and runs clean under the address and undefined-behaviour sanitizers. *(Revised after the first
CI run:)* the random generator draws directly from `std::mt19937`'s output, which the standard
fixes, instead of the standard distributions, which differ between libraries, and never relies
on the unspecified order of function arguments, so every platform tests the same expressions.
Where a law's two sides contain a floating-point number (an exact result that overflowed 64
bits, D7), they are compared in value at a few points instead of in structure.

**D16. `Function` holds an array of outputs, compiled together.** A function has an ordered
domain and outputs of any shape (scalar, vector, matrix, …), stored row-major. All outputs share
one compiled program, so work common to several outputs, such as Jacobian entries, is done once.
`grad()` appends a dimension of the domain's size: a scalar's gradient is a vector, a vector's
gradient is its Jacobian, and `hessian()` is `grad().grad()`. `f[i]` slices the first
dimension (a Jacobian's row). Results go into buffers you provide (`std::span`), with
convenience overloads returning `std::vector`, so LODE does not impose a tensor type.

**D17. Evaluation runs a compiled program in a simple interpreter.** Compilation lowers the
outputs to a flat list of instructions, then assigns registers. While lowering, it:

- computes equal subexpressions once, even when they are different nodes (keyed by structure);
- emits identical instructions once (value numbering), with `a + b` and `b + a` recognized;
- builds integer powers by repeated squaring from shared smaller powers (`x^2` serves `x^3`);
- folds operations on constants, and gives instructions constant operands (`x*2.5 + y` is one
  `MulAddConst`) so constants never occupy registers;
- multiplies a product's negative powers together and divides once;
- applies a product's coefficient last, so `3*x*y` and `5*x*y` share `x*y`.

Registers are reused as soon as a value has been read for the last time, which keeps the
register file small (44 registers for the 1021-instruction Jacobian benchmark). The interpreter
is a `switch` in a loop over contiguous instructions: no recursion, no virtual calls.

**D18. Batch evaluation processes blocks of 32 points.** Each register holds a value for every
point in the block, and each instruction loops over the block. The cost of dispatching an
instruction is spread across 32 points, and the inner loops are simple enough to vectorize.
In the benchmarks, batches are 2.5–4.6× faster per point than one point at a time.

**D19. Functions compile when built and are immutable afterwards.** Building may be slow
(R1). Copies share the compiled program, so copying is cheap. Each thread evaluates with its
own scratch registers (`thread_local`), so one function can be evaluated from several threads
at once without locking.

**D20. Partial evaluation by argument.** For `f(x, y, z)`, `f(a, b, c)` with any symbolic
argument substitutes the arguments for the variables and compiles the result. The new domain
is the variables of the arguments, in order of first appearance: `f(x, 2, z)` is a function of
`(x, z)`, `f(z, y, x)` swaps two variables, `f(2*t, y, 1 - t)` reparametrizes to `(t, y)`. An
integer argument stays exact; a floating-point one (`2.5`) becomes a real constant.
`f.partial({{y, 2}})` does the same by naming variables. Constants fold during
substitution, so the partially evaluated function has fewer instructions than the original.

**D21. `f(x, y) = expr` is not supported.** In nablaxx, `f(x, y, z) = ...` set the domain. In
LODE, `f(x, y, z)` is partial evaluation and returns a new function, so assigning to it would
silently change a temporary. `Function::operator=` only works on a named function, which makes
that line a compile error. Write `Function f(x, y, z); f = ...;` or `Function f({x, y, z}, expr)`.

**D22. A variable outside the domain and parameters is an error.** Building a function whose
outputs use a variable that is neither in its domain nor among its parameters throws, naming
the variable and both lists. *(Revised by D23: originally the domain only.)*

**D23. Parameters.** A function may have parameters besides its domain: inputs whose values
are given at each evaluation and that derivatives leave alone. Both partial evaluation and
parameters are substitution of a symbol; they differ in *when*. Partial evaluation substitutes
when the function is built (constants fold; fastest evaluation; a rebuild per change of value,
38 µs for the Robertson Jacobian, about 370 evaluations). Parameters substitute at each
evaluation (no rebuild). One rule keeps them simple: a function's inputs are its domain's
variables followed by its parameters; calls and `evaluate` take all inputs in that order (or
the parameters separately, `evaluate(point, parameters, out)`), `grad` differentiates with
respect to the domain only, and `diff(parameter)` still gives a sensitivity. Partial evaluation
applies to parameters too, so values that never change can be folded while others stay
parameters. In a batch, all points can share one set of parameter values.

**D24. Results go into `std::span` buffers now; a tensor container comes later.** You chose
compatibility first: spans let LODE write into a `std::vector`, `std::array`, MATL storage, or
another library's arrays without copying. A tensor container that wraps contiguous results and
indexes them with any rank, as nablaxx's `Tensor` was meant to, will follow. MATL adapters
(dense `Matrix`, sparse `CSRMatrix`) remain possible on top.

**D25. Code generation writes the optimized program as plain C++.** `generate_cpp(function,
name)` returns a self-contained header (it needs no LODE at run time) that defines, as templates
on the number type, `name(point[, parameters], outputs)`, `name(inputs, outputs)` when there are
parameters, and `name_batch(...)`. The body is the compiler's single-assignment program, one
`T const` variable per value, so every optimization of D17 is kept and the C++ compiler assigns
machine registers itself. Inputs are named after their variables where those are usable C++
identifiers (otherwise `input_k`), distinct constants are named once (`c0`, `c1`, …), and the
generated code compiles warning-free under `-Wall -Wextra -Werror`. The intended workflow
generates code during the build: a small program builds the functions and calls `write_cpp`, a
CMake custom command runs it, and the program that needs the functions compiles the header.
LODE's own tests and benchmark use exactly this workflow.

**D26. Lowering and register assignment are separate steps.** `Compiler::lower_outputs` produces
the single-assignment program, now with unused instructions removed (dead code elimination);
`allocate_registers` turns it into an interpreter `Program`. Code generation uses the first step
only.

**D27. Indexing convention: `()` from 1 for mathematical objects, `[]` from 0 for arrays.**
Parentheses take one or more indices counting from 1, as in the mathematics and in MATL;
square brackets take one index counting from 0, an offset into storage, as in the C++ standard
library. A future tensor container will follow both: `A(i, j)` for the mathematical entry,
`A[k]` for the k-th element of its storage. For: the bracket shows at a glance which convention
is in effect, which matters where both meet (a sparse matrix's entries and its storage arrays);
standard C++ is moving the same way (C++23's `std::mdspan` indexes `m[i, j]` from 0);
`operator()` takes several indices in C++20; index 0 with `()` is always an error, which catches
a forgotten conversion; and MATL and LODE share one rule. Against: Eigen, Armadillo, Blaze and
xtensor use `A(i, j)` from 0, so users of those libraries must be told; and code at the boundary
between the two conventions converts with `i - 1`, though now visibly. In LODE, `()` on a
`Function` already means evaluation, so a function cannot be indexed with it.
`Function::operator[]` (a 0-based `[]` on a mathematical object) will be removed; how to reach a
function's components is decided with the design question in Open questions.

**D28. Public repository with continuous integration.** LODE is public at
github.com/DanielFust/lode (public repositories run GitHub-hosted CI without using Actions
minutes). `.github/workflows/ci.yml` builds and runs every test and example on each push to
`main` and each pull request: GCC 14 and Clang 18 on Linux, Apple Clang on macOS, Clang with
the address and undefined-behaviour sanitizers, and a GCC Release build that also runs the
benchmark.

**D29. Powers of numbers have one form.** A rational number raised to a fractional power with
no exact value is normalized: a fractional base is split into numerator and denominator, and
the integer part of the exponent is taken out, leaving an exponent between 0 and 1
(`2^(3/2) = 2*2^(1/2)`, `2^(-1/2) = 2^(1/2)/2`, `(2/3)^(1/2) = 2^(1/2)*3^(1/2)/3`). This is
SymPy's rule. Without it, `(2^(-1/2))^3` and `2^(-1/2)*(2^(-1/2))^2` were equal numbers in
different forms. Found by the extended seed sweep after the first CI run.

**D30. Functions have tensor-valued outputs; tensors are indexed, functions are applied.**
Decided with you after weighing both designs. A `Function` stays the compiled unit, with outputs
of any shape compiled together, because sharing work between outputs (Jacobian entries above all)
is LODE's main source of speed, and because a Jacobian needs one common domain. A matrix of
separately compiled scalar functions would lose that sharing and repeat the domain in every
entry. Components are reached through the outputs: `F.outputs()(i, j)` is an expression, indexed
from 1 (D27), from which a new function can be built. Calling a function with numbers,
`f(1.0, 2.0)`, stays the main use. To come: a `Tensor<T>` container (contiguous, any rank,
`A(i, j, ...)` from 1 and `A[k]` from 0) for expressions, symbols and numeric results; `outputs()`
returning a `Tensor<Expr>`; evaluation that can return a `Tensor<double>`; and domains shaped as
tensors, for which `grad()` appends the domain's whole shape (a scalar energy of a 3x3 matrix
variable has a 3x3 gradient). The gradient's derivative index comes last: J_ij = dF_i/dx_j.
`Function::operator[]` is removed in favour of this.

**D31. The tensor container.** `Tensor<T>` holds any rank in contiguous row-major storage:
`t(i, j, ...)` is a checked entry counted from 1, `t[k]` an unchecked storage element counted
from 0 (D27). Construction follows `std::vector`: parentheses give dimensions
(`Tensor<double> a(2, 3)`), braces give elements (`{x, y}`, `{{a, b}, {c, d}}`), and a runtime
shape is the explicit type `Shape` (`Tensor<int>(Shape{2, 2}, elements)`). Allowing a shape as a
braced list would have made `Tensor<double> a({3, 4})` silently mean the two-element vector
[3, 4], because braced lists prefer element-list constructors. In `Function`, the domain is a
`Tensor<Symbol>` (a braced list `{x, y}` is a rank-1 tensor, so existing code is unchanged), the
outputs a `Tensor<Expr>`, and `evaluate(inputs)` returns a `Tensor<double>`. `make_symbols("F",
{2, 2})` creates F11 to F22 (with underscores, F_10_1, if a dimension exceeds 9). A scalar
function called with numbers, `f(1.0, 2.0)`, still returns a `double`: returning a tensor would
allocate on every call of the most common path. `Function::operator[]` is removed (D27, D30).

**D32. Every constant factor of a product and constant term of a sum is combined.** The compiler
folded constants only where they happened to be adjacent, so `x*(3/2)^20*e*sqrt(2)` took three
multiplications. It now multiplies all constant factors into one number (including a product's
share of its sum term's coefficient, which still uses a multiply-add) and adds all constant
terms once. Found while answering your question about exact numbers and evaluation speed.

**D33. Exact numbers stay exact; overflow will be promoted to arbitrary precision.** Agreed
with you: keep numbers exact (free at evaluation, D32, and needed for terms to cancel), and,
before high-order derivatives or high-degree polynomials become a use case, promote results
that outgrow 64 bits to arbitrary-precision integers instead of `double`.

**D34. Sparse matrix functions.** `SparseMatrixFunction` stores a matrix-valued function by its
nonzero entries: the pattern in compressed sparse row form (`row_offsets`, `column_indices`,
counted from 0 as storage offsets, D27), and the entries' values as an ordinary vector
`Function`, `values()`, so evaluation, batches, parameters and code generation all work on it
unchanged. `entry(i, j)` gives an entry as an expression, counted from 1. `F.sparse_jacobian()`
differentiates each output only with respect to the variables that appear in it, and drops
derivatives that simplify to zero, so the pattern is exact and a large system never builds its
dense Jacobian. `SparseMatrixFunction(F)` makes any matrix-valued function sparse.
`generate_cpp(sparse, name)` writes the values like any function plus the pattern as
`constexpr std::array`s, so a program can set up its sparse matrix without LODE.

**D35. MATL interoperability is optional.** `include/lode/matl.hpp` (included only by programs
that use MATL) provides `csr_matrix(sparse)`, a MATL `CSRMatrix` with the same pattern into
whose `values()` LODE evaluates in place (both libraries use 0-based CSR arrays and 1-based
entries), and `evaluate(matrix_function, inputs, matl_matrix)` for dense matrices (both
row-major). LODE does not depend on MATL. With `-DLODE_WITH_MATL=ON`, CMake uses an installed
MATL or fetches v2.2.0 from GitHub and builds `matl_test`; a sixth CI job does this.

## Log

### 2026-10-26 — Review of nablaxx

- The repository did not compile: the half-written rule-based simplifier had about 20 errors,
  and a typo (`estimate_cpu_cyles`) left classes without a required override.
- With the simplifier removed in a scratch copy, the original demo ran and differentiated
  correctly, but `2*2*x` was not folded, `y*x - x*y` was not zero, `x + y + x` was not `2*x + y`,
  and `x*y + x` came out as `x*y + x^2` (a wrong answer from the factoring rule).
- Evaluating a function at a point (`f(2.0, 3.0)`) did not compile.
- Diagnosis: binary trees and pairwise rules (see D4), no canonical order, and rules that undo
  each other (factoring versus distributing). Recommended a rebuild around canonical forms.

### 2026-10-26 — Name and repository

- Brainstormed names, then checked your shortlist for clashes (D2). Chose LODE.
- Created `~/Documents/GitHub/lode`: local only for now, BSD 3-Clause (R5), CMake 3.20+,
  header-only interface target `lode::lode`, and a dependency-free test harness
  (`tests/check.hpp`: `LODE_CHECK`, `LODE_CHECK_EQUAL`, `LODE_CHECK_THROWS`). Commit `e4ae3d2`.

### 2026-10-26 — Phase 1: expression core (`a55ce30`)

- `Number` (D7), `Expr` and `Symbol` (D5, D6, D8), canonical construction of sums, products and
  powers (D4), and printing (`x/(y*z)`, `(x + y)^2`, `x^(1/2)`).
- The canonical order sorts a power next to its base (`x`, `x^2`, then `y`), then calls,
  products and sums, which makes printed sums read naturally.
- Added D10 after the randomized associativity test failed on `(2*(x + y))*z`.
- Tests: numbers, canonical forms, printing, and a randomized property test (about 24,000
  checks per seed, 20 seeds).

### 2026-10-26 — Design discussion

- Explained the variant design (D5) and shared nodes (D6).
- On the cost of branching: a correctly predicted branch is almost free and a mispredicted one
  costs roughly 15–20 cycles. In a tree-walking evaluator the larger costs are memory (each node
  a separate allocation, so pointer-chasing cache misses) and recomputing shared
  subexpressions. The compiled evaluator addresses both; batch evaluation (many points per
  instruction) makes the remaining dispatch cost negligible.
- You set R1 and R2, and left the order of features to me.

### 2026-10-26 — Phase 2: calculus and evaluation (`0838c4e`)

- A sixth node kind, `Call`, for `exp`, `log`, `sin`, `cos`, with the automatic rules of D11.
  Exact arguments stay symbolic (`exp(1)` is the exact number e); real arguments are evaluated.
- `diff(expr, x, order)`: sum, product and chain rules, and the general power rule
  (`d/dx x^x = x^x*(log(x) + 1)`). Each shared node is differentiated once.
- `substitute(expr, {{x, value}, ...})`: simultaneous replacement; unchanged parts are reused,
  changed parts rebuilt in canonical form, so numbers fold (`x*y + y^2` at `y = 2` is
  `2*x + 4`). This is the core of partial evaluation (R2).
- `symbols(expr)` lists an expression's variables.
- `evaluate<T>(expr, {{x, 1.0}, ...})`: the reference evaluator (D14). Integer powers use repeated
  multiplication rather than `std::pow`, which keeps complex-step derivatives accurate for
  negative bases.
- Added D12 after the randomized test of `exp(e)*exp(f) = exp(e + f)` failed when `e` was a
  logarithm.
- Tests: functions, derivatives (including about 2,900 random complex-step comparisons per seed,
  agreeing to 1e-9), substitution, evaluation; the property test now includes functions.

### 2026-10-26 — Development log started (`66b9ceb`)

- You asked for this log. It lives in the repository so that each phase's entry is committed
  with its code.

### 2026-10-26 — Phase 3: `Function` and the compiled evaluator

- `Function` (D16, D19–D22) with `evaluate`, `evaluate_batch`, `operator()` for scalars, `grad`,
  `hessian`, `diff`, slicing with `[]`, partial evaluation, and `listing()` to show the program.
- The compiler and interpreter (D17, D18) are in `include/lode/detail/compiler.hpp` and
  `program.hpp`. As an example, partially evaluating your demo function, `f(x, 2, z)`, gives
  `x + 2*x^2 + 2*x*z + 3`, which compiles to:

  ```
  r2 = r0*r0          x^2 (r0 holds x, r1 holds z)
  r2 = r2*2.0 + r0    2*x^2 + x
  r1 = r0*r1          x*z, reusing z's register
  r2 = r1*2.0 + r2    + 2*x*z
  r2 = r2 + 3.0       + 3
  out[0] = r2
  ```

- Tests (`tests/function_test.cpp`): your demo, its gradient and Hessian; the Robertson stiff
  kinetics right-hand side and its Jacobian against a hand-derived one; batch against pointwise
  evaluation; partial evaluation, renaming and reparametrization; instruction counts proving
  shared work (`(x + y)^2 + (x + y)^3` is 5 instructions); errors and their messages. On random
  expressions, the compiled evaluator agrees with the reference evaluator to 1e-12, and
  compiled gradients agree with complex-step derivatives taken *through the compiled program*,
  which also exercises it with complex numbers. 20 extra seeds pass.
- A benchmark (`bench/benchmark.cpp`, built with `-DLODE_BUILD_BENCHMARKS=ON`). Nanoseconds per
  point, Release build, Apple Clang 21:

  | Function | Instructions | Registers | Reference | Compiled | Batch | By hand |
  | --- | ---: | ---: | ---: | ---: | ---: | ---: |
  | Robertson right-hand side | 11 | 4 | 4,500 | 27 | 10.5 | 9.2 |
  | Robertson Jacobian (3×3) | 15 | 3 | 8,000 | 33 | 10.8 | 6.4 |
  | Network right-hand side (30 species) | 134 | 37 | 193,000 | 249 | 100 | |
  | Network Jacobian (30×30) | 1,021 | 44 | 4,534,000 | 3,040 | 659 | |
  | Triangle P2 shape functions (6) | 23 | 5 | 6,600 | 47 | 14.3 | 4.5 |
  | Triangle P2 gradients (6×2) | 25 | 6 | 8,900 | 51 | 15.3 | 5.6 |

  Reading the table: compiled evaluation is 140–1,500× faster than the reference evaluator,
  and batches are another 2.5–4.6× faster per point. Hand-written C++ remains 1.1–3.2× faster
  than batches on these small functions, because the interpreter still dispatches each
  instruction. Generating C++ from a `Function` (phase 5) would remove that cost entirely for
  functions known when your program is built. The network Jacobian is mostly zeros; 900 of its
  1,021 instructions are stores, one per entry.

### 2026-10-26 — Discussion of the open questions

- **Sparse Jacobians: yes, eventually**, possibly with MATL's sparse types. MATL's `CSRMatrix`
  exposes its values as a `std::span` and its index arrays are 0-based, so a sparse Jacobian
  could compile only the nonzero entries, in CSR order, and write them straight into
  `S.values()`, with the pattern built once. Canonical forms make "identically zero" an exact
  test, so the pattern is exact.
- **Parameters: undecided.** Your model so far was substitution of a symbol. Both are
  substitution; the difference is *when*. Partial evaluation substitutes when the function is
  built, so constants fold and evaluation is fastest, but every change of value means a
  rebuild. Parameters would defer the substitution to each evaluation: no rebuild, and
  derivatives exclude them. Measured on the Robertson Jacobian with a temperature-dependent
  rate: fixing `T` by partial evaluation takes 38 µs, while one evaluation takes 0.1 µs, so a
  rebuild costs about 370 evaluations. That matters when a value changes every time step
  (temperature in a non-isothermal reactor, element geometry in finite elements). Proposed
  API sketched in the conversation; awaiting your decision.
- **Result container: undecided.** Options weighed: standard types (`std::span` in,
  `std::vector` out), a LODE tensor type, or MATL types. MATL stores dense matrices row-major
  (as LODE orders outputs) with a `data()` pointer, and already uses `std::span` for its CSR
  arrays, so a span-based core can write into MATL storage without copying. Proposed: keep the
  span-based core and add an optional MATL adapter header; awaiting your decision.

### 2026-10-26 — Decisions on the open questions; parameters

- **Sparse Jacobians:** agreed, for later, possibly with MATL's sparse types (status table).
- **Parameters:** you approved them as an addition (D23), though you may not use them much.
  My finite-element example (element geometry as parameters) was off: shape functions are
  differentiated with respect to the reference element's natural coordinates, and the geometry
  enters through the Jacobian of the coordinate mapping. To be discussed in a later project.
- **Results:** `std::span` now, a tensor container later (D24).
- **Indexing:** you set out the principle recorded as R6. `Function::operator[]` (slicing a
  function's outputs) is currently 0-based; whether a function's outputs count as a
  mathematical object is a question for that discussion.
- Implemented parameters: `lode::Parameters`, the new constructor argument, `parameters()`,
  `evaluate(point, parameters, out)`, `evaluate_batch(points, parameters, out)` (parameters
  copied to every point of a block), and partial evaluation and `partial` over parameters. Error
  messages name both lists: `'T' is not in the domain (cA) or the parameters (A)`.
- Tests (`tests/parameters_test.cpp`): the Arrhenius example; separate and combined parameter
  values agree with each other and with substituting before building; changing temperature
  without rebuilding; folding some parameters and keeping one; sensitivities; batches with
  shared parameters; errors. All 11 test programs pass on Clang, GCC and the sanitizers;
  benchmark timings are unchanged.

### 2026-10-27 — Phase 4: examples

- You redirected phase 4: start with basic examples of usage and core features, include
  chemistry as an application, and leave out finite elements (too specialized for a general
  audience).
- Nine programs in `examples/`, following MATL's conventions (numbered files, a header saying
  what each shows and how to run it, sections with dividers, an index in
  `examples/README.md`):
  1. expressions and automatic simplification; 2. powers, roots and elementary functions, and
  which simplifications are automatic; 3. differentiation; 4. substitution and evaluation;
  5. `Function`, gradients, Hessians, Jacobians and the compiled program; 6. partial evaluation;
  7. parameters; 8. batch evaluation; 9. Robertson's stiff kinetics integrated with backward
  Euler and Newton's method, using the exact Jacobian with the rate constants as parameters.
- Every expected result is written as a comment beside the code that prints it, and each was
  checked against the actual output.
- The examples build by default and run under CTest (`LODE_BUILD_EXAMPLES`), so they cannot go
  stale: 20 tests now (11 test programs and 9 examples).
- The chemistry example reproduces the reference solution at t = 40 to about 0.05% (first-order
  backward Euler, about 2,000 steps from t = 0 to 40,000) and conserves total concentration to
  1e-15.
- Writing the examples turned up one compiler improvement: a sum now starts from a term with
  coefficient 1, so `x*z - y` compiles to one subtraction instead of a negation and an addition.

### 2026-10-27 — Phase 5a: C++ code generation, and an explainer

- `include/lode/codegen.hpp`: `generate_cpp` and `write_cpp` (D25), on top of the separated
  lowering step (D26). See the README for the workflow.
- Tests: a generator program (`tests/codegen_generator.cpp`) writes code for 129 functions
  during the build (Robertson with parameters, edge cases such as a constant function, unused
  inputs, special powers, and variables named `double`, `v0`, `a` twice and `x'`, plus 60 random
  functions and their Jacobians: 6,700 lines). `codegen_test` compiles it with warnings as errors
  and checks it against the interpreter in double and complex arithmetic and in batches, and
  checks the generated text and the errors for unusable names.
- Found and fixed a bug while testing: NaN and infinity constants were written as
  `std::numeric_limits<T>::...`, which is not defined for `std::complex` and silently gives 0.
  They are now built from `double` and converted, as the interpreter does.
- Benchmark (Release, Apple Clang 21), nanoseconds per point, now with generated code:

  | Function | Interpreter | Batch | Generated | Hand-written |
  | --- | ---: | ---: | ---: | ---: |
  | Robertson right-hand side | 32 | 11.4 | 4.7 | 5.8 |
  | Robertson Jacobian (3×3) | 43 | 11.0 | 5.7 | 6.7 |
  | Network right-hand side (30) | 278 | 99 | 26 | |
  | Network Jacobian (30×30) | 3,190 | 827 | 486 | |
  | Triangle P2 shape functions | 53 | 15.8 | 4.9 | 5.1 |
  | Triangle P2 gradients | 57 | 17.6 | 6.4 | 6.4 |

  Generated code is as fast as hand-written code, which was the goal of R1. The network Jacobian
  is limited by writing 900 mostly-zero entries; sparse Jacobians will address that.
- You asked for a document explaining the optimizations, for your own learning:
  [optimizations.md](optimizations.md) walks through each one with real listings from LODE,
  explains interpreter dispatch, batches, code generation and the testing techniques, and lists
  further reading. Writing it, I corrected one claim of my own: repeated squaring is faster than
  `std::pow` but can be slightly *less* accurate for large exponents, not more.
- Queue agreed with you: the indexing discussion, then sparse Jacobians, then a discussion of
  what the gradient of a vector of functions should be (the question that originally led you to
  write nablaxx's Tensor). Continuous integration to follow once the repository is published.

### 2026-10-27 — Indexing settled; published with CI

- Indexing discussion: you proposed `()` from 1 and `[]` from 0, I gave the arguments for and
  against, and we agreed (D27). You also confirmed that a tensor container's `[k]` gives a 0-based
  flat offset into storage.
- You raised the deeper question this leads to: should LODE's functions have tensor-valued
  domains and codomains, or should a function always be scalar-valued, with a Jacobian being a
  matrix of functions? Discussed in the conversation; the decision will be recorded here.
- Published the repository and added CI (D28). The first CI run's result is recorded below
  once it finishes.

### 2026-10-27 — First CI run, and what it found

- Of five CI jobs, GCC 14 on Linux and Apple Clang on macOS passed, and three failed:
  - **GCC 14, Release:** a `-Wnonnull` warning (an error under `-Werror`), from copying zero
    parameters out of a null pointer in `Program::run`. Harmless, but GCC 15 had not flagged it.
    `run` with all inputs in one array no longer goes through that path.
  - **Clang 18, Debug and sanitizers:** the randomized test of `pow(e, 2)*e == pow(e, 3)`
    failed. It failed on Linux Clang but not elsewhere because the random expressions differed
    by platform: the standard random distributions are implemented differently by libstdc++ and
    libc++, and a fraction's numerator and denominator were drawn as two arguments of one call,
    whose order of evaluation is unspecified.
- Sweeping 300 seeds locally (I had swept 20) found occasional failures of three laws on both
  compilers. Two causes: (1) exact rationals overflowing 64 bits into floating point, after which
  results depend on rounding order (by design, D7; see Known limitations); (2) a real gap in the
  canonical form, powers of numbers with fractional exponents (fixed by D29).
- Fixes: D29; the random generator made identical on every platform, and the property test
  compares in value where overflow made numbers inexact (D15). After them, all 300 seeds pass
  on both compilers, and the derivative and compiled-function tests pass over 30 seeds each.
- You decided the function design (D30), and are aligned on starting it.

### 2026-10-27 — CI green; phase 6: tensors

- The CI run for the fixes passed all five jobs.
- `include/lode/tensor.hpp`: `Shape`, `Tensor<T>` and `make_symbols` (D31). `Function` now uses
  tensors for its domain, outputs and values; `grad()` appends the domain's shape, so the
  gradient of a scalar function of a matrix is a matrix, and the derivative index comes last
  (J(i, j) = dF_i/dx_j). `operator[]` on functions is gone; tests and examples index
  `outputs()` instead.
- Tests: `tests/tensor_test.cpp` covers construction, both kinds of indexing and their
  errors, printing, `make_symbols`, the Jacobian's index order, a quadratic form (gradient
  (A + A^T)x, Hessian A + A^T), the determinant of a 2x2 matrix variable (gradient = cofactor
  matrix; Hessian of rank 4), evaluation to tensors, and how partial evaluation treats a shaped
  domain. The code generation test gained a determinant, its gradient and its Hessian.
- Example 10 (`examples/10_tensors.cpp`) shows tensors and functions of vectors and matrices.
  Writing it caught a mistake of mine worth knowing about: looping over
  `det.hessian().shape()` refers into a temporary that is destroyed before the loop body runs
  (see Known limitations).

### 2026-10-27 — Exact numbers and evaluation speed; constants fully folded

- You confirmed: the temporary-lifetime limitation is fine for now, and LODE stays at C++20
  (R7).
- You asked whether keeping large constants exact, such as (3/2)^82, could make evaluation
  slower, and whether expensive symbolic constants could arise during simplification. Findings,
  from experiments on real output:
  - Exact constants cost nothing at evaluation, because compiling folds them into numbers.
    Testing this found that folding was incomplete across separate factors and terms; fixed
    (D32), so `x*(3/2)^20*e*sqrt(2) + log(3)` now compiles to one multiplication and one
    addition.
  - Overflowing into floating point is worse than slow: it stops terms cancelling.
    `x*(3/2)^82*(2/3)^82` gives `0.9999999999999953*x` instead of `x`, and
    `(y*(3/2)^82 + x) - y*(3/2)^41*(3/2)^41` leaves a spurious `-0.03125*y` term instead of `x`,
    so the compiled program does extra work and gives a slightly wrong result.
  - Where it arises: high-order derivatives (the 20th derivative of x^25 has the coefficient
    25!/5!, about 1.3e23, past 64 bits), high-degree polynomials with rational coefficients,
    and large exponents in rate laws.
  - My recommendation is in Open questions.

### 2026-10-27 — Phase 7: sparse Jacobians and MATL

- You agreed on exact numbers (D33). Sparse Jacobians: D34; MATL interoperability: D35.
- Robertson's Jacobian stores 7 of 9 entries; the 30-species benchmark network 132 of 900.
  Benchmark (Release, Apple Clang 21, nanoseconds per point), dense versus sparse network
  Jacobian: interpreter 2,882 versus 507, batch 689 versus 178, generated code 448 versus 77.
- Tests: `tests/sparse_test.cpp` (the pattern, values against the dense Jacobian, construction
  from a dense matrix function, a 200-species chain, a one-row Jacobian, exact cancellation, 200
  random functions, and errors); the code generation test now also generates sparse Jacobians
  and checks the generated pattern arrays and values; `tests/matl_test.cpp` (built with MATL)
  writes a sparse Jacobian into a MATL `CSRMatrix`, compares it with a dense MATL matrix, and
  multiplies it by a vector with MATL.
- Example 11 (`examples/11_sparse_jacobians.cpp`): a chain of reversible reactions with a
  tridiagonal Jacobian. The explainer gained a section on sparsity.
- Also fixed on the way: a GCC 15 warning about a temporary in the constant-folding change, by
  naming the factor list (a false positive, but the code is clearer).

## Known limitations

- `shape()`, `outputs()` and `domain()` return references into the function. Looping over one
  taken from a temporary, as in `for (auto d : f.hessian().shape())`, uses a destroyed object:
  C++20 does not extend the temporary's life through the loop (C++23 does). Keep the function
  in a variable first.

- The algebraic laws hold exactly only while numbers stay exact: rationals have 64-bit parts,
  and a result that overflows becomes a `double` (D7), after which equal values can differ in
  their last digit. Arbitrary-precision integers would remove this; see Open questions.
- Powers of different integers are not combined or split: `6^(1/2)` and `2^(1/2)*3^(1/2)` are
  equal but have different forms (that would need integer factorization).

- Constants are folded in real arithmetic, so when evaluating in complex numbers, `log(-1)` is
  NaN rather than iπ (the same in the interpreter and in generated code). Complex evaluation is
  meant for complex-step derivatives of real functions, where this does not matter.

- The interpreter is 1.1–3.2× slower than hand-written C++ in batch mode, and 3–10× slower one
  point at a time (see the phase 3 benchmark). C++ code generation is planned (phase 5).
- Outputs are dense: a sparse Jacobian still stores every zero entry at each evaluation. Large
  reaction networks would benefit from sparse output.

- Products of sums are not expanded (D9), so `x*(y + z)` does not equal `x*y + x*z`.
- Simplifications that are not valid for every real value are not automatic (D11): no
  `sqrt(x^2) = x`, no `log(x^2) = 2*log(x)`, no trigonometric identities such as
  `sin(x)^2 + cos(x)^2 = 1`.
- `exp(y*log(x))` stays as written rather than becoming `x^y`; only logarithm terms with a
  numeric coefficient are taken out of an exponential.
- Sums with floating-point coefficients are not made primitive (D10), so the grouping
  guarantee holds only for exact coefficients.
- Requires GCC or Clang (D3).
- Some lines run to 101–103 columns: over the 100-column aim, under the 120-column limit.

## Open questions for you

None at the moment.
