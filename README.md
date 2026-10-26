# LODE

**Library for Operators, Derivatives and Expressions**

LODE is a header-only C++20 library for building mathematical functions as symbolic
expressions, differentiating them exactly, and evaluating them fast. It grew out of the
`nablaxx` prototype.

What works today (phase 1): expressions are built in canonical form as you write them.

```cpp
#include "lode/lode.hpp"

lode::Symbol x("x"), y("y"), z("z");
lode::Expr f = x*x*2 + 1 + y + x + x*y*z;   // x + 2*x^2 + y + x*y*z + 1

x + y + x;                // 2*x + y
y*x - x*y;                // 0
x*y + x;                  // x + x*y
(2*x + 2)/(x + 1);        // 2
pow(sqrt(x), 2);          // x
x/2 + x/3;                // 5*x/6 (exact rationals)
```

The goal, still to come: write the math, get exact derivatives, evaluate quickly.

```cpp
lode::Function f(x, y, z);
f = x*x*2 + 1 + y + x + x*y*z;
auto grad_f = f.grad();           // [4*x + y*z + 1, x*z + 1, x*y]
double value = f(1.0, 2.0, 3.0);
```

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
  operations, with common subexpressions computed once.

## Roadmap

1. Expression core: symbols, numbers, sums, products and powers in canonical form. *(Done.)*
2. Differentiation, substitution, and `exp`, `log`, `pow`, `sqrt`, `sin`, `cos`.
3. `Function` with evaluation, gradient, Jacobian and Hessian.
4. Examples as tests: finite-element shape functions and a stiff chemistry Jacobian.
5. Later: expand, collect, and C++ code generation.

## Building

LODE needs CMake 3.20 or newer and a C++20 compiler; it uses the GCC/Clang `__int128`
extension. It is tested with GCC 15 and Apple Clang 21, warnings as errors.

```bash
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

To use LODE from another CMake project, add it with `add_subdirectory` and link
`lode::lode`.

## License

BSD 3-Clause. See [LICENSE](LICENSE).
