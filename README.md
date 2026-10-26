# LODE

**Library for Operators, Derivatives and Expressions**

LODE is a header-only C++20 library for building mathematical functions as symbolic
expressions, differentiating them exactly, and evaluating them fast. It grew out of the
`nablaxx` prototype.

```cpp
// The goal: write the math, get exact derivatives, evaluate quickly.
lode::Symbol x("x"), y("y"), z("z");
lode::Function f(x, y, z);
f = x*x*2 + 1 + y + x + x*y*z;    // stored as 2*x^2 + x + y + x*y*z + 1

auto grad_f = f.grad();           // [4*x + y*z + 1, x*z + 1, x*y]
double value = f(1.0, 2.0, 3.0);
```

*(Planned interface; not implemented yet.)*

## Intended uses

- Shape functions and their derivatives for finite elements.
- Right-hand sides and exact Jacobians for systems of ODEs, such as stiff reaction kinetics.

## Design

- **Canonical forms.** Sums and products are stored as flat, sorted collections, so building
  an expression merges like terms and folds constants: `x + y + x` becomes `2*x + y`, and
  `y*x - x*y` becomes `0`. Most simplification happens automatically, without a rule engine.
- **Immutable, shared expression nodes.** Subexpressions are shared rather than copied, which
  keeps derivatives small.
- **Explicit transforms.** Expansion, collection and factoring are separate operations that
  you ask for.
- **Compiled evaluation.** A function, gradient or Jacobian is compiled into a flat list of
  operations, with common subexpressions computed once.

## Roadmap

1. Expression core: symbols, numbers, sums, products and powers in canonical form.
2. Differentiation, substitution, and `exp`, `log`, `pow`, `sqrt`, `sin`, `cos`.
3. `Function` with evaluation, gradient, Jacobian and Hessian.
4. Examples as tests: finite-element shape functions and a stiff chemistry Jacobian.
5. Later: expand, collect, and C++ code generation.

## Building

LODE needs CMake 3.20 or newer and a C++20 compiler (GCC 11+, Clang 14+).

```bash
cmake -S . -B build && cmake --build build && ctest --test-dir build
```

To use LODE from another CMake project, add it with `add_subdirectory` and link
`lode::lode`.

## License

BSD 3-Clause. See [LICENSE](LICENSE).
