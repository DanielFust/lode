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
| 3 | `Function`: compiled fast evaluation, batches, gradient/Jacobian/Hessian, partial evaluation | In progress | |
| 4 | Worked examples as tests: finite-element shape functions, stiff chemistry Jacobian | Planned | |
| 5 | `expand`, `collect`, C++ code generation | Planned | |

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
also run over 20 extra seeds before a phase is committed. Every phase builds warning-free on
Clang and GCC and runs clean under the address and undefined-behaviour sanitizers.

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

### 2026-10-26 — Phase 3 plan: `Function` and the compiled evaluator

Plan, to be updated as the work proceeds:

- `Function` binds an ordered domain of symbols to one or more output expressions with a shape
  (scalar, vector, matrix), like nablaxx's `Tensor<Function>` but compiled together.
- Compilation turns all outputs into one flat list of instructions with common subexpressions
  computed once, integer powers built from shared smaller powers (`x^2` reused for `x^3`),
  constants folded, and registers reused once a value is no longer needed.
- Evaluation runs that list in a loop: no recursion, no virtual calls, contiguous memory. A batch
  mode evaluates many points per instruction.
- `grad()` adds a dimension to the shape (scalar → gradient vector, vector → Jacobian matrix);
  `hessian()` is the gradient of the gradient.
- Partial evaluation: `f(x, 2, z)` substitutes and recompiles, giving `g(x, z)`.

## Known limitations

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

- **Container for tensor results.** Plan: LODE writes results into a buffer you provide
  (`std::span<double>`), with convenience overloads returning `std::vector<double>`, so it can
  plug into MATL or anything else. Would you prefer direct MATL types?
- **Variables outside the domain.** Plan: a `Function` whose expressions contain a variable not in
  its domain (say a rate constant `k`) is an error, so values must be substituted first. The
  alternative is to treat such variables as parameters supplied at evaluation.
- **GitHub.** The repository is local only. Say when you want it pushed, and whether public or
  private.
