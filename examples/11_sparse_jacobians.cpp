/*--------------------------------------------------------------------------------------------------
 * File: 11_sparse_jacobians.cpp
 *
 * Description: Example 11: sparse Jacobians.
 *
 *              In a large system each equation usually involves only a few of the variables, so
 *              most entries of the Jacobian are zero. sparse_jacobian() finds which entries are
 *              not zero, once, and computes only those, in compressed sparse row (CSR) form.
 *
 *              The system here is a chain of reversible reactions A1 <-> A2 <-> ... <-> An, whose
 *              Jacobian is tridiagonal.
 *
 *              Build and run from the repository root:
 *
 *                  cmake -S . -B build && cmake --build build
 *                  ./build/examples/11_sparse_jacobians
 *------------------------------------------------------------------------------------------------*/
#include <iostream>
#include <string>
#include <vector>

#include "lode/lode.hpp"

using namespace lode;

int main()
{
    /*---------------------------------------- The system ----------------------------------------*/

    // Concentrations a1, ..., an; forward rate constant kf and backward rate constant kb as
    // parameters
    std::size_t const n = 6;
    Tensor<Symbol> a = make_symbols("a", {n});
    Symbol kf("kf");
    Symbol kb("kb");

    // Reaction i turns A_i into A_(i+1) at rate kf*a_i - kb*a_(i+1)
    std::vector<Expr> rates(n, Expr(0));
    for (std::size_t i = 1; i < n; ++i)
    {
        Expr rate = kf*a(i) - kb*a(i + 1);
        rates[i - 1] -= rate;
        rates[i]     += rate;
    }
    Function rhs(a, rates, {kf, kb});

    /*----------------------------------- The sparse Jacobian ------------------------------------*/

    SparseMatrixFunction jacobian = rhs.sparse_jacobian();
    std::cout << n << "x" << n << " Jacobian with " << jacobian.nonzero_count()
              << " nonzero entries:\n";
    std::cout << jacobian << "\n\n";                   // {(1, 1): -kf, (1, 2): kb, (2, 1): kf, ...}

    // Entries count from 1, as in the mathematics; an entry that is not stored is zero
    std::cout << "entry (2, 2) = " << jacobian.entry(2, 2) << "\n";       // -kb - kf
    std::cout << "entry (1, 3) = " << jacobian.entry(1, 3) << "\n\n";     // 0

    // The pattern in CSR form. These arrays hold storage positions, so they count from 0: row
    // i's entries are positions row_offsets[i] up to row_offsets[i + 1], and column_indices
    // gives each entry's column.
    std::cout << "row_offsets    =";
    for (std::size_t offset : jacobian.row_offsets()) std::cout << " " << offset;
    std::cout << "\ncolumn_indices =";
    for (std::size_t column : jacobian.column_indices()) std::cout << " " << column;
    std::cout << "\n\n";

    /*------------------------------ Evaluating the nonzero entries ------------------------------*/

    // values() is an ordinary vector function of the nonzero entries, in the order of the
    // pattern: evaluate it, batch it, or generate C++ from it like any other function
    std::vector<double> concentrations{1.0, 0.5, 0.25, 0.125, 0.0625, 0.03125};
    std::vector<double> constants{2.0, 0.5};
    std::vector<double> values(jacobian.nonzero_count());
    jacobian.evaluate(concentrations, constants, values);
    std::cout << "values         =";
    for (double value : values) std::cout << " " << value;
    std::cout << "\n";
    std::cout << "instructions: " << jacobian.values().instruction_count()
              << " for the sparse Jacobian, " << rhs.grad().instruction_count()
              << " for the dense one\n";

    // With MATL, the pattern goes into a sparse matrix once and evaluation writes its values in
    // place (include "lode/matl.hpp"):
    //
    //     matl::CSRMatrix<double> J = lode::csr_matrix(jacobian);
    //     jacobian.evaluate(concentrations, constants, J.values());
    //
    // And generate_cpp(jacobian, "chain_jacobian") writes the values as C++, with the pattern as
    // constant arrays (chain_jacobian_row_offsets, chain_jacobian_column_indices).

    return 0;
}
