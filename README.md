# LODE

[![CI](https://github.com/DanielFust/lode/actions/workflows/ci.yml/badge.svg)](https://github.com/DanielFust/lode/actions/workflows/ci.yml)

**Library for Operators, Derivatives and Expressions**

LODE is a header-only C++20 library for building mathematical functions as symbolic
expressions, differentiating them exactly, and evaluating them fast. It grew out of the
`nablaxx` prototype.

What works today: expressions are built in canonical form as you write them, and can be
differentiated, substituted into, and evaluated.

```cpp
#include "lode/lode.hpp"

lode::Symbol x("x"), y("y"), z("z");
lode::Expr f = x*x*2 + 1 + y + x + x*y*z;   // x + 2*x^2 + y + x*y*z + 1

// Canonical form simplifies as you build
x + y + x;                  // 2*x + y
y*x - x*y;                  // 0
(2*x + 2)/(x + 1);          // 2
exp(x)*exp(y);              // exp(x + y)
x/2 + x/3;                  // 5*x/6 (exact rationals)

// Derivatives
diff(f, x);                 // 4*x + y*z + 1
diff(sin(x*x), x);          // 2*x*cos(x^2)

// Substitution, the basis of partial evaluation
substitute(f, {{y, 2}});    // x + 2*x^2 + 2*x*z + 3

// Reference evaluation (simple and checked; Function below is the fast path)
lode::evaluate(f, {{x, 1.0}, {y, 2.0}, {z, 3.0}});   // 12.0
```

Functions bind variables to inputs and compile for fast evaluation:

```cpp
lode::Function f(x, y, z);
f = x*x*2 + 1 + y + x + x*y*z;
double value = f(1.0, 2.0, 3.0);    // 12
lode::Function grad_f = f.grad();   // [4*x + y*z + 1, x*z + 1, x*y]
lode::Function hess_f = f.hessian(); // [[4, z, y], [z, 0, x], [y, x, 0]]
lode::Function g = f(x, 2, z);      // partial evaluation: g(x, z) = x + 2*x^2 + 2*x*z + 3

lode::Expr d2 = grad_f.outputs()(2);      // x*z + 1: outputs are a tensor, indexed from 1
lode::Tensor<double> g_at = grad_f.evaluate(std::vector<double>{1.0, 2.0, 3.0});   // [11, 4, 2]
grad_f.evaluate_batch(points, results);   // many points at once, into your own storage
```

Vector functions work the same way; the gradient of a vector function is its Jacobian:

```cpp
lode::Function rhs({y1, y2, y3}, {-k1*y1 + k3*y2*y3,
                                  k1*y1 - k3*y2*y3 - k2*pow(y2, 2),
                                  k2*pow(y2, 2)});
lode::Function jacobian = rhs.grad();     // 3x3, compiled together; J(i, j) = dF_i/dy_j
```

Variables can be arranged as a tensor too. The gradient appends the variables' shape, so a
scalar function of a matrix has a matrix gradient:

```cpp
lode::Tensor<lode::Symbol> F = lode::make_symbols("F", {2, 2});       // F11, F12, F21, F22
lode::Function det(F, F(1, 1)*F(2, 2) - F(1, 2)*F(2, 1));
det.grad();                               // [[F22, -F21], [-F12, F11]], the cofactor matrix
det.hessian();                            // a tensor of rank 4
```

Indexing follows one convention throughout: `()` gives a mathematical entry, counting from 1
(`A(1, 2)`), and `[]` gives an element of storage, counting from 0 (`A[k]`). Functions
themselves are applied with `()`, not indexed; their outputs and values are tensors.

Parameters are inputs that derivatives leave alone, for values that change between
evaluations without rebuilding the function:

```cpp
lode::Expr k = A*exp(-Ea/(R*T));                                 // Arrhenius rate
lode::Function rhs({cA, cB}, {-k*cA*cB, -k*cA*cB}, {T, A, Ea});  // domain, outputs, parameters
lode::Function jac = rhs.grad();             // 2x2: derivatives with respect to cA, cB only
jac.evaluate(state, parameter_values, J);    // T, A, Ea given at each call
lode::Function jac_T = jac.partial({{A, 1e7}, {Ea, 5e4}});   // fold the constants, keep T
```

For the fastest evaluation, LODE can write a function as C++ source, to compile into your
program. The generated header is self-contained: it needs no LODE at run time.

```cpp
#include "lode/codegen.hpp"
lode::write_cpp(jacobian, "robertson_jacobian", "generated/robertson_jacobian.hpp");

// in your program, built with the generated header:
#include "robertson_jacobian.hpp"
robertson_jacobian(state.data(), rate_constants.data(), J.data());   // plain C++, takes pointers
```

How the evaluation is made fast is explained, with examples, in
[docs/optimizations.md](docs/optimizations.md).

## Examples

[`examples/`](examples) has short, annotated programs: expressions and simplification,
functions, differentiation, substitution, `Function`, partial evaluation, parameters, batch
evaluation, and an application to stiff chemical kinetics. See
[examples/README.md](examples/README.md).

## Intended uses

- Shape functions and their derivatives for finite elements.
- Right-hand sides and exact Jacobians for systems of ODEs, such as stiff reaction kinetics.

## Design

- **Canonical forms.** Sums and products are stored as flat, sorted collections, so building
  an expression merges like terms and folds constants: `x + y + x` becomes `2*x + y`, and
  `y*x - x*y` becomes `0`. Most simplification happens automatically, without a rule engine.
  Products of sums are left unexpanded; an explicit `expand` is planned.
- **Exact numbers.** Coefficients are exact rationals (`1/3`, not `0.333...`); a value that
  overflows 64 bits becomes a `double`. Floating-point constants such as `2.5` stay distinct
  from exact ones.
- **Immutable, shared expression nodes.** Subexpressions are shared rather than copied, which
  keeps derivatives small.
- **Explicit transforms.** Expansion, collection and factoring are separate operations that
  you ask for.
- **Compiled evaluation.** A function, gradient or Jacobian is compiled into a flat list of
  operations, with common subexpressions computed once and registers reused. Batch evaluation
  runs each operation over a block of points. See `bench/benchmark.cpp` and the timings in
  [docs/devlog.md](docs/devlog.md).

## Roadmap

1. Expression core: symbols, numbers, sums, products and powers in canonical form. *(Done.)*
2. Differentiation, substitution, `exp`, `log`, `sin`, `cos`, and a reference evaluator.
   *(Done.)*
3. `Function` with fast compiled evaluation (including batches of points), gradient,
   Jacobian, Hessian, and partial evaluation. *(Done.)*
4. Examples of usage and core features, and a stiff chemical kinetics application. *(Done.)*
5. C++ code generation *(done)*; `expand` and `collect`.
6. A tensor container; functions with tensor-valued domains and outputs. *(Done.)*
7. Sparse Jacobians, written into MATL's sparse matrices.

## Building

LODE needs CMake 3.20 or newer and a C++20 compiler; it uses the GCC/Clang `__int128`
extension. It is tested with GCC 15 and Apple Clang 21, warnings as errors.

```bash
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

To use LODE from another CMake project, add it with `add_subdirectory` and link
`lode::lode`. To build the benchmark, configure a Release build with
`-DLODE_BUILD_BENCHMARKS=ON` and run `lode_benchmark`.

Design decisions, progress and known limitations are recorded in
[docs/devlog.md](docs/devlog.md).

## License

BSD 3-Clause. See [LICENSE](LICENSE).
