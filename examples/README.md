# LODE examples

Short, annotated programs, from the basics to an application. Each prints its results, with
the expected output written beside the code that produces it.

| Example | What it shows |
| --- | --- |
| [01_expressions](01_expressions.cpp) | Symbols, building expressions, automatic simplification, exact numbers, equality |
| [02_powers_and_functions](02_powers_and_functions.cpp) | Powers, roots, `exp` `log` `sin` `cos`, and which simplifications are automatic |
| [03_differentiation](03_differentiation.cpp) | Derivatives: rules, higher orders, partial and mixed derivatives |
| [04_substitution_and_evaluation](04_substitution_and_evaluation.cpp) | Substituting numbers and expressions, listing variables, numeric evaluation |
| [05_functions](05_functions.cpp) | `Function`: evaluation, gradients, Hessians, vector functions, Jacobians, the compiled program |
| [06_partial_evaluation](06_partial_evaluation.cpp) | Fixing, renaming and reparametrizing variables to make new functions |
| [07_parameters](07_parameters.cpp) | Parameters: inputs whose values change between evaluations, without rebuilding |
| [08_batch_evaluation](08_batch_evaluation.cpp) | Evaluating many points in one call |
| [09_chemical_kinetics](09_chemical_kinetics.cpp) | Application: integrating Robertson's stiff reactions with an exact Jacobian |
| [10_tensors](10_tensors.cpp) | Tensors: indexing from 1 with `()` and from 0 with `[]`, functions of vectors and matrices |

## Building and running

The examples build with the rest of LODE when it is the top-level project:

```bash
cmake -S . -B build && cmake --build build
```

Each one is then `build/examples/<name>`, for instance `./build/examples/01_expressions`.
They also run as part of `ctest --test-dir build`, which checks that they still work. For the
timings in example 8, configure a Release build (`-DCMAKE_BUILD_TYPE=Release`).

To compile one directly:

```bash
c++ -std=c++20 -I include examples/01_expressions.cpp -o expressions
```
