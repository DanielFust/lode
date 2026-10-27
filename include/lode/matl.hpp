/*--------------------------------------------------------------------------------------------------
 * File: matl.hpp
 *
 * Description: Optional interoperability with MATL (github.com/DanielFust/MATL): LODE's sparse
 *              Jacobians as MATL sparse matrices, and matrix-valued functions evaluated into MATL
 *              dense matrices. Include it only in programs that also use MATL; LODE itself does
 *              not depend on MATL.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_MATL_HPP
#define LODE_MATL_HPP

#include <cstddef>
#include <span>
#include <stdexcept>
#include <string>
#include <vector>

#include <matl.hpp>

#include "lode/lode.hpp"

namespace lode
{
   /*-----------------------------------------------------------------------------------------------
    * matl::CSRMatrix<T, I> csr_matrix(SparseMatrixFunction const& matrix)
    *
    * A MATL sparse matrix with the function's sparsity pattern, its values zero. Create it once;
    * then, at each point, write the function's values straight into it, since a MATL sparse
    * matrix exposes its values as a std::span in the same order:
    *
    *     matl::CSRMatrix<double> J = lode::csr_matrix(jacobian);
    *     jacobian.evaluate(state, J.values());
    *
    * Both libraries store compressed sparse rows counted from 0, so the pattern is copied as it
    * is.
    *
    * Template Parameters:
    *     T         Value type of the matrix
    *     I         Index type of the matrix's arrays
    *---------------------------------------------------------------------------------------------*/
    template<class T = double, class I = matl::index_t>
    matl::CSRMatrix<T, I> csr_matrix(SparseMatrixFunction const& matrix)
    {
        std::vector<I> row_offsets(matrix.row_offsets().begin(), matrix.row_offsets().end());
        std::vector<I> column_indices(matrix.column_indices().begin(), matrix.column_indices().end());
        std::vector<T> values(matrix.nonzero_count(), static_cast<T>(0));
        return matl::CSRMatrix<T, I>(static_cast<matl::index_t>(matrix.rows()),
                                     static_cast<matl::index_t>(matrix.columns()),
                                     std::move(row_offsets), std::move(column_indices),
                                     std::move(values));
    }


   /*-----------------------------------------------------------------------------------------------
    * void evaluate(Function const& matrix, std::span<double const> inputs, matl::Matrix<double>& out)
    *
    * Evaluates a matrix-valued function into a MATL dense matrix of the same dimensions. Both
    * store a matrix row by row, so the values are written in place.
    *
    * Throws std::invalid_argument if the function's outputs are not a matrix of out's
    * dimensions.
    *---------------------------------------------------------------------------------------------*/
    inline void evaluate(Function const& matrix, std::span<double const> inputs,
                         matl::Matrix<double>& out)
    {
        std::vector<std::size_t> const& shape = matrix.shape();
        bool fits = shape.size() == 2 && shape[0] == static_cast<std::size_t>(out.rows())
                 && shape[1] == static_cast<std::size_t>(out.cols());
        if (!fits)
        {
            throw std::invalid_argument("lode::evaluate: the function's outputs are not a "
                                        + std::to_string(out.rows()) + "x" + std::to_string(out.cols())
                                        + " matrix");
        }
        matrix.evaluate(inputs, std::span<double>(out.data(), static_cast<std::size_t>(out.size())));
    }
}
#endif
