# How LODE makes evaluation fast

A short tour of the optimizations in LODE, each with real output from the library, for
learning. The listings come from `Function::listing()`, which prints the compiled program: `r0`,
`r1`, … are registers (inputs start in `r0`, `r1`, … in domain order) and `out[j]` is output
`j`.

LODE works in three stages, and each stage removes a different kind of waste:

```
  expressions ──build──▶ canonical form ──compile──▶ instruction list ──run──▶ numbers
  (you write)           (simplified, shared)        (optimized)              interpreter, batch,
                                                                             or generated C++
```

---

## 1. Simplify before computing

The cheapest instruction is one that is never emitted. Every expression is built directly in
**canonical form** (see `include/lode/detail/canonical.hpp`): like terms and powers combine and
numbers fold as the expression is built, so `(2*x + 2)/(x + 1)` is the number `2` before any
compiling happens.

Expressions are **shared, not copied**. The product rule gives `f'*g + f*g'`, and the `f` and
`g` in it are the same nodes as in the original expression, so derivatives (and Jacobians and
Hessians) do not repeat whole subtrees. Differentiation also remembers each node's derivative,
so a shared subexpression is differentiated once.

## 2. Compute each value once

**Common subexpression elimination.** The compiler keys each subexpression by its *structure*
(its hash and canonical form), not by where it sits in memory, so equal subexpressions compile
to one value even when they were built separately. Here `x*y` appears in both outputs and is
computed once:

```
[sin(x*y), cos(x*y)]             5 instructions, 2 registers
r1 = r0*r1          x*y
r0 = sin(r1)
out[0] = r0
r1 = cos(r1)        reuses x*y
out[1] = r1
```

**Value numbering.** At the instruction level, the compiler also keeps a table of the
instructions it has emitted, keyed by operation and operands, and reuses an existing result
instead of emitting a duplicate. Operands of `+` and `*` are put in a fixed order first, so
`a*b` and `b*a` are recognized as the same. This catches repeats that only appear after
lowering, such as the `x*x` inside both `x^2` and `x^4`.

## 3. Powers by repeated squaring

`x^n` is computed with about log₂ n multiplications by squaring: `x^8 = ((x^2)^2)^2`. Each
power of each base is remembered, so the powers in one function share their work:

```
x^2 + x^3 + x^5 + x^8            9 instructions (5 multiplications)
r1 = r0*r0          x^2
r2 = r0*r1          x^3 = x*x^2
r2 = r1 + r2
r1 = r1*r1          x^4 = (x^2)^2
r0 = r0*r1          x^5 = x*x^4
r0 = r2 + r0
r1 = r1*r1          x^8 = (x^4)^2
r1 = r0 + r1
out[0] = r1
```

Computing each power separately would take 1 + 2 + 4 + 7 = 14 multiplications. Finding the
fewest multiplications for a set of powers is the *addition chain* problem; repeated squaring
with shared intermediate powers is simple, and not far from optimal for the small exponents
typical in practice. Integer powers never call `std::pow`, which is much slower than a few
multiplications. (Repeated multiplication rounds at each step, so for large exponents it can
lose a few bits of accuracy compared with a good `std::pow`; for the small exponents of
polynomials and rate laws the difference is negligible.)

## 4. Fold constants, and make them part of instructions

**Constant folding.** Anything that depends only on numbers is computed once, when compiling.
Exact constants such as `sqrt(2)`, `e` and `log(3)` stay symbolic in expressions (so algebra
with them stays exact), and become numbers only now. All the constant factors of a product, and
all the constant terms of a sum, are combined into a single number each, however they arose:

```
log(3) + 2^(1/2)*x + x^2*exp(1)        5 instructions
r1 = r0*r0                         x^2
r0 = r0*1.4142135623730951         sqrt(2)*x
r0 = r1*2.718281828459045 + r0     e*x^2 + sqrt(2)*x, one multiply-add
r0 = r0 + 1.0986122886681098       + log(3)
out[0] = r0
```

This is why keeping numbers exact costs nothing at evaluation: a product such as
`x*(3/2)^20*e*sqrt(2)` compiles to one multiplication by 12783.055…, the same as if you had
written the number yourself.

**Constant operands.** Instructions carry their constant (`AddConst`, `MulConst`,
`MulAddConst`, …), so constants never take a register or a load. A sum with coefficients becomes
a chain of multiply-adds, starting from a term with coefficient 1 so that subtraction is one
instruction:

```
2*x - y + z/2 + 7
r0 = r0*2.0
r1 = r0 - r1
r1 = r2*0.5 + r1    multiply-add with a constant
r1 = r1 + 7.0
out[0] = r1
```

**Partial evaluation is constant folding on a larger scale.** Fixing a variable substitutes its
value and rebuilds the expression, so everything that depended on it folds. In computer science
this is called *specialization*: a general program, given some of its inputs early, becomes a
faster program for the rest.

```
f(x, y, z) = x*y + x*exp(y) + y^2*z      8 instructions
f(x, 2.0, z) = 9.38905609893065*x + 4.0*z      2 instructions + store
```

## 5. Prefer cheap operations

Not all arithmetic costs the same. On recent processors a double-precision multiplication or
addition has a latency of about 3–4 cycles, and a new one can start every half cycle or so; a
division has a latency of roughly 10–15 cycles and a new one can start only every few cycles.
`exp`, `log`, `sin`, `cos` and `pow` are library calls of tens of cycles. So the compiler:

- turns division by a constant into multiplication by its reciprocal (`x/3` is `x*0.333…`);
- collects all of a product's negative powers and **divides once**: `x/(y*z)` is one
  multiplication and one division, not two divisions;
- applies a product's coefficient **last**, so that products differing only in their
  coefficient share the work:

```
[3*x*y, 5*x*y*z]
r1 = r0*r1          x*y
r0 = r1*3.0
out[0] = r0
r1 = r2*r1          z*(x*y): reuses x*y
r1 = r1*5.0
out[1] = r1
```

Multiplying by a reciprocal can differ from true division in the last bit; the tests compare
the compiled evaluator with the reference evaluator to a relative tolerance of 1e-12.

## 6. Few registers: single assignment, then reuse

The compiler first writes the program in **static single assignment** form: every value gets
its own name and is assigned exactly once. That makes the optimizations above simple: a value
means one thing everywhere.

Then it **assigns registers** (`allocate_registers` in `compiler.hpp`). Walking the
instructions in order, it notes the last instruction that reads each value; after that
instruction, the value's register is free and the next new value takes it. This is a simple
form of *linear-scan register allocation*. The effect: the 30×30 Jacobian in the benchmark has
1,021 instructions but needs only 44 registers, which fit easily in the processor's fastest
cache. A result may even take the register of its own operand (`r1 = r1*r1` above), because
every operation reads its operands before it writes.

Instructions whose results are never read are removed by a backward pass (*dead code
elimination*); in single assignment form one pass finds them all.

## 7. The interpreter

`Program::execute` in `program.hpp` is a `switch` inside a loop over a contiguous array of
instructions. There is no recursion and there are no virtual calls, and the instructions are
read in order from memory, which processors handle well.

The remaining cost is *dispatch*: for every instruction, the processor must jump to the right
`case`. It predicts where the jump goes, and a correct prediction is almost free, but programs
mix operations unpredictably, so some predictions fail, at about 15–20 cycles each. Values also
live in an array in memory rather than in the processor's registers. Together these make the
interpreter several times slower than equivalent compiled code on small functions.

## 8. Batches: share the dispatch cost

`evaluate_batch` runs each instruction over a block of 32 points before moving to the next
instruction. Each register holds 32 values, one per point, stored next to each other (a
*structure of arrays*). The dispatch cost is now paid once per 32 points, and the inner loop
(`r[l] = a[l] * b[l]` for each point `l`) is simple enough for the C++ compiler to vectorize,
processing several points per machine instruction. In the benchmark below, batches are about
3–4× faster per point than single points.

## 9. Code generation: remove the interpreter

`generate_cpp` (in `include/lode/codegen.hpp`) prints the single-assignment program as C++
source, one `T const` variable per value:

```cpp
template<class T>
inline void example([[maybe_unused]] T const* point, T* outputs)
{
    T const x = point[0];
    T const y = point[1];
    T const c0 = static_cast<T>(3.0);
    T const c1 = static_cast<T>(5.0);
    T const v0 = x * y;
    T const v1 = v0 * c0;
    outputs[0] = v1;
    T const v2 = x * x;
    T const v3 = x * v2;
    T const v4 = y * v3;
    T const v5 = v4 * c1;
    outputs[1] = v5;
}
```

All of LODE's optimizations are already in this code. Your C++ compiler then does what an
interpreter cannot: keeps every value in a machine register, schedules the instructions to
overlap their latencies, can fuse multiplications and additions into single instructions, and
inlines the function into the caller's loop.
There is nothing left to dispatch. The generated code uses single-assignment form, not LODE's
register assignment, because the C++ compiler assigns machine registers better itself.

Nanoseconds per point, Release build, Apple Clang 21:

| Function | Interpreter | Batch | Generated | Hand-written |
| --- | ---: | ---: | ---: | ---: |
| Robertson right-hand side | 32 | 11.4 | 4.7 | 5.8 |
| Robertson Jacobian (3×3) | 43 | 11.0 | 5.7 | 6.7 |
| Network right-hand side (30) | 278 | 99 | 26 | |
| Network Jacobian (30×30) | 3,190 | 827 | 486 | |
| Triangle P2 shape functions | 53 | 15.8 | 4.9 | 5.1 |
| Triangle P2 gradients | 57 | 17.6 | 6.4 | 6.4 |

Generated code is as fast as the hand-written versions. Differences of a nanosecond or so are
near the timing noise; where generated code is slightly ahead, part of the reason is that it
computes each shared subexpression once by construction (in the Robertson right-hand side,
`k3*y2*y3` and `y2^2` each appear in two formulas), while hand-written code relies on the C++
compiler to notice the repeats.

## 10. Sparsity: do not compute zeros

In large systems, each equation involves only a few variables, so most Jacobian entries are zero.
`sparse_jacobian()` finds the nonzero entries *symbolically*, once: each output is differentiated
only with respect to the variables that appear in it, and a derivative that simplifies to exactly
zero is dropped. Canonical form makes "exactly zero" a reliable test (`y1*y2 - y2*y1` is `0`),
so the pattern is exact rather than an over-estimate. The nonzero entries become an ordinary
vector function, compiled with all the optimizations above, and the pattern is stored in
compressed sparse row (CSR) form, the format sparse linear solvers expect. For the 30-species
network in the benchmark, 132 of the Jacobian's 900 entries are nonzero, and every way of
evaluating it gets about 4–6× faster:

| Network Jacobian | Interpreter | Batch | Generated |
| --- | ---: | ---: | ---: |
| Dense (900 entries, 1,021 instructions) | 2,882 | 689 | 448 |
| Sparse (132 entries, 253 instructions) | 507 | 178 | 77 |

## 11. How the optimizations are checked

Optimizations are only useful if they keep the results right. Three techniques do the checking:

- **A reference evaluator.** `evaluate` simply walks the expression tree. It is slow but easy to
  trust, and the compiled evaluator and the generated code are compared against it on thousands
  of random expressions.
- **Randomized algebraic laws.** Random expressions are combined in different orders and
  groupings, and the results must be *identical*: `(e*f)*g == e*(f*g)`, `e*f/f == e`,
  `exp(e)*exp(f) == exp(e + f)`, and so on. This found two design gaps that fixed examples had
  missed (decisions D10 and D12 in the development log).
- **Complex-step derivatives.** For a real function f, the Taylor series gives
  f(x + ih) = f(x) + ih f′(x) − h²f″(x)/2 + …, so f′(x) ≈ Im f(x + ih)/h with an error of order
  h². Unlike a finite difference, nothing is subtracted, so there is no cancellation and h can be
  as small as 1e-20: the result is exact to machine precision. LODE's evaluators work with any
  number type, so running them on `std::complex<double>` checks every symbolic derivative.

## Further reading

- K. D. Cooper and L. Torczon, *Engineering a Compiler*: value numbering, static single
  assignment form, and register allocation, the core of sections 2 and 6.
- M. Poletto and V. Sarkar, "Linear scan register allocation", *ACM Transactions on Programming
  Languages and Systems*, 1999: the register allocation method of section 6, in full.
- D. E. Knuth, *The Art of Computer Programming*, vol. 2, section 4.6.3, "Evaluation of
  powers": addition chains and repeated squaring.
- M. A. Ertl and D. Gregg, "The structure and performance of efficient interpreters", *Journal of
  Instruction-Level Parallelism*, 2003: why interpreter dispatch is costly, and the techniques
  that reduce it.
- N. D. Jones, C. K. Gomard and P. Sestoft, *Partial Evaluation and Automatic Program
  Generation*, 1993: partial evaluation as a general technique.
- W. Squire and G. Trapp, "Using complex variables to estimate derivatives of real functions",
  *SIAM Review*, 1998: the complex-step method.
- A. Griewank and A. Walther, *Evaluating Derivatives*, 2nd ed., 2008: automatic
  differentiation, the main alternative to symbolic differentiation for computing exact
  derivatives.
- A. Fog, *Instruction tables*: measured latencies and throughputs of arithmetic instructions
  on many processors (agner.org/optimize).
