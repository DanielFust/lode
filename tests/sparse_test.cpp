/*--------------------------------------------------------------------------------------------------
 * File: sparse_test.cpp
 *
 * Description: Tests of sparse matrix functions and sparse Jacobians: the pattern, the values,
 *              agreement with the dense Jacobian, and errors.
 *------------------------------------------------------------------------------------------------*/
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "check.hpp"
#include "lode/lode.hpp"
#include "random_expressions.hpp"

namespace
{
    using lode::Expr;
    using lode::Function;
    using lode::SparseMatrixFunction;
    using lode::Symbol;


   /*-----------------------------------------------------------------------------------------------
    * bool matches_dense(SparseMatrixFunction const& sparse, Function const& dense,
    *                    std::vector<double> const& inputs)
    *
    * Whether a sparse matrix function equals a dense matrix function: the same entries as
    * expressions, and the same values at a point.
    *---------------------------------------------------------------------------------------------*/
    bool matches_dense(SparseMatrixFunction const& sparse, Function const& dense,
                       std::vector<double> const& inputs)
    {
        if (dense.shape() != std::vector<std::size_t>{sparse.rows(), sparse.columns()}) return false;
        for (std::size_t i = 1; i <= sparse.rows(); ++i)
            for (std::size_t j = 1; j <= sparse.columns(); ++j)
                if (sparse.entry(i, j) != dense.outputs()(i, j)) return false;

        std::vector<double> values(sparse.nonzero_count());
        sparse.evaluate(inputs, values);
        lode::Tensor<double> full = dense.evaluate(inputs);
        for (std::size_t i = 0; i < sparse.rows(); ++i)
        {
            for (std::size_t p = sparse.row_offsets()[i]; p < sparse.row_offsets()[i + 1]; ++p)
            {
                double expected = full[i * sparse.columns() + sparse.column_indices()[p]];
                if (std::isnan(expected) && std::isnan(values[p])) continue;
                double tolerance = 1e-12 * std::max(1.0, std::abs(expected));
                if (std::abs(values[p] - expected) > tolerance) return false;
            }
        }
        return true;
    }
}


int main()
{
    /*----------------------------------- Robertson's kinetics -----------------------------------*/

    Symbol y1("y1");
    Symbol y2("y2");
    Symbol y3("y3");
    Symbol k1("k1");
    Symbol k2("k2");
    Symbol k3("k3");
    Function rhs({y1, y2, y3},
                 {-k1*y1 + k3*y2*y3, k1*y1 - k3*y2*y3 - k2*pow(y2, 2), k2*pow(y2, 2)},
                 {k1, k2, k3});
    SparseMatrixFunction jacobian = rhs.sparse_jacobian();

    // Entries (3, 1) and (3, 3) are zero, so 7 of 9 are stored
    LODE_CHECK_EQUAL(jacobian.rows(), std::size_t{3});
    LODE_CHECK_EQUAL(jacobian.columns(), std::size_t{3});
    LODE_CHECK_EQUAL(jacobian.nonzero_count(), std::size_t{7});
    LODE_CHECK(jacobian.row_offsets() == (std::vector<std::size_t>{0, 3, 6, 7}));
    LODE_CHECK(jacobian.column_indices() == (std::vector<std::size_t>{0, 1, 2, 0, 1, 2, 1}));
    LODE_CHECK_EQUAL(jacobian.entry(3, 2), 2*k2*y2);
    LODE_CHECK_EQUAL(jacobian.entry(3, 1), Expr(0));
    LODE_CHECK_EQUAL(jacobian.values().parameters().size(), std::size_t{3});

    // The same as the dense Jacobian, and the same as building it from the dense Jacobian
    std::vector<double> inputs{0.9, 2e-5, 0.1, 0.04, 3e7, 1e4};
    LODE_CHECK(matches_dense(jacobian, rhs.grad(), inputs));
    SparseMatrixFunction from_dense(rhs.grad());
    LODE_CHECK(from_dense.row_offsets() == jacobian.row_offsets());
    LODE_CHECK(from_dense.column_indices() == jacobian.column_indices());
    LODE_CHECK(jacobian.dense().outputs() == rhs.grad().outputs());

    // Values at a point, with the parameters given separately
    std::vector<double> state{0.9, 2e-5, 0.1};
    std::vector<double> rates{0.04, 3e7, 1e4};
    std::vector<double> values(7);
    jacobian.evaluate(state, rates, values);
    LODE_CHECK(std::abs(values[4] - (-6e7 * 2e-5 - 1e4 * 0.1)) < 1e-9);

    // Printing lists the entries, counted from 1
    std::ostringstream text;
    text << SparseMatrixFunction(Function({y1, y2}, {{y1, Expr(0)}, {Expr(0), y1*y2}}));
    LODE_CHECK_EQUAL(text.str(), std::string("{(1, 1): y1, (2, 2): y1*y2}"));

    /*-------------------------------- A long chain of reactions ---------------------------------*/

    // A_1 -> A_2 -> ... -> A_n: a bidiagonal Jacobian with 2n - 1 nonzeros
    std::size_t const n = 200;
    std::vector<Symbol> a;
    for (std::size_t i = 0; i < n; ++i) a.emplace_back("a" + std::to_string(i));
    std::vector<Expr> rates_of_change(n, Expr(0));
    for (std::size_t i = 0; i + 1 < n; ++i)
    {
        rates_of_change[i]     -= a[i];
        rates_of_change[i + 1] += a[i];
    }
    SparseMatrixFunction chain = Function(a, rates_of_change).sparse_jacobian();
    LODE_CHECK_EQUAL(chain.nonzero_count(), 2*n - 2);
    LODE_CHECK_EQUAL(chain.entry(1, 1), Expr(-1));
    LODE_CHECK_EQUAL(chain.entry(2, 1), Expr(1));
    LODE_CHECK_EQUAL(chain.entry(n, n), Expr(0));
    LODE_CHECK_EQUAL(chain.values().instruction_count(), 2*n - 2);

    /*--------------------------------------- Other shapes ---------------------------------------*/

    // A scalar function has a one-row Jacobian: its gradient's nonzeros
    Function product({y1, y2, y3}, y1*y3);
    SparseMatrixFunction row = product.sparse_jacobian();
    LODE_CHECK_EQUAL(row.rows(), std::size_t{1});
    LODE_CHECK(row.column_indices() == (std::vector<std::size_t>{0, 2}));

    // Cancellation is exact, so a derivative that simplifies to zero is not stored
    Function cancelling({y1, y2}, {y1*y2 - y2*y1 + y2});
    LODE_CHECK_EQUAL(cancelling.sparse_jacobian().nonzero_count(), std::size_t{1});

    // Random vector functions: sparse and dense Jacobians agree
    lode::test::RandomExpressions random(20261030, {y1, y2, y3}, true);
    for (int trial = 0; trial < 200; ++trial)
    {
        Function f({y1, y2, y3}, {random.next(3), random.next(3), random.next(2)});
        LODE_CHECK(matches_dense(f.sparse_jacobian(), f.grad(), {0.7, 1.3, 1.9}));
    }

    /*------------------------------------------ Errors ------------------------------------------*/

    LODE_CHECK_THROWS(rhs.grad().sparse_jacobian(), std::logic_error);
    LODE_CHECK_THROWS(SparseMatrixFunction(rhs), std::invalid_argument);
    LODE_CHECK_THROWS(jacobian.entry(0, 1), std::out_of_range);
    LODE_CHECK_THROWS(jacobian.entry(1, 4), std::out_of_range);
    Function two_values({y1}, {y1, 2*y1});
    LODE_CHECK_THROWS(SparseMatrixFunction(2, 2, {0, 1, 1}, {0}, two_values), std::invalid_argument);
    using Pattern = std::vector<std::size_t>;
    LODE_CHECK_THROWS(SparseMatrixFunction(2, 2, Pattern{0, 2, 2}, Pattern{1, 0}, two_values),
                      std::invalid_argument);
    LODE_CHECK_THROWS(SparseMatrixFunction(2, 2, Pattern{0, 1, 2}, Pattern{0, 2}, two_values),
                      std::invalid_argument);

    return lode::test::report();
}
