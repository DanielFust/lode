/*--------------------------------------------------------------------------------------------------
 * File: sparse.hpp
 *
 * Description: Sparse matrix functions: matrix-valued functions stored by their nonzero entries,
 *              with the sparsity pattern in compressed sparse row (CSR) form. Sparse Jacobians
 *              of large systems, such as reaction networks, are the main use.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_SPARSE_HPP
#define LODE_SPARSE_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

#include "lode/function.hpp"

namespace lode
{
   /*-----------------------------------------------------------------------------------------------
    * class SparseMatrixFunction
    *
    * A matrix-valued function stored by its nonzero entries. The sparsity pattern (which entries
    * are not identically zero) is found once, symbolically, and kept in compressed sparse row
    * (CSR) form:
    *
    *     row_offsets()       rows() + 1 positions: row i's entries are the positions
    *                         row_offsets()[i] to row_offsets()[i + 1] - 1
    *     column_indices()    each entry's column, increasing within a row
    *
    * Both arrays hold storage offsets, so they count from 0 (as in MATL, SciPy and most sparse
    * libraries). The entries' values are an ordinary vector Function, values(), whose outputs are
    * the nonzero entries in CSR order: it is compiled, evaluated, batched and turned into C++ like
    * any other function, and computes nothing for the zero entries. The mathematical entry (i, j)
    * is entry(i, j), counted from 1.
    *
    * A Jacobian has an entry wherever an output depends on a variable, and the pattern stays
    * fixed while the values change, so a solver can set up its sparse matrix once.
    *---------------------------------------------------------------------------------------------*/
    class SparseMatrixFunction
    {
      private:
        std::size_t              rows_;
        std::size_t              columns_;
        std::vector<std::size_t> row_offsets_;    // CSR row positions, from 0
        std::vector<std::size_t> column_indices_; // CSR columns, from 0, increasing in each row
        Function                 values_;         // The nonzero entries, in CSR order

      public:
        // The entries of a matrix-valued function that are not identically zero; throws
        // std::invalid_argument if the function's outputs are not a matrix
        explicit SparseMatrixFunction(Function const& matrix);

        // A sparse matrix function from its pattern and its nonzero entries' values; throws
        // std::invalid_argument if the pattern is not valid CSR or does not match the values
        SparseMatrixFunction(std::size_t rows, std::size_t columns,
                             std::vector<std::size_t> row_offsets,
                             std::vector<std::size_t> column_indices, Function values);

        /*--------------------------------------- Queries ----------------------------------------*/

        std::size_t rows() const {return this->rows_;}
        std::size_t columns() const {return this->columns_;}
        std::size_t nonzero_count() const {return this->column_indices_.size();}
        std::vector<std::size_t> const& row_offsets() const {return this->row_offsets_;}
        std::vector<std::size_t> const& column_indices() const {return this->column_indices_;}

        // The nonzero entries as a vector function, in CSR order
        Function const& values() const {return this->values_;}

        // The expression of entry (i, j), counted from 1: 0 if the entry is not stored. Throws
        // std::out_of_range for an index outside the matrix.
        Expr entry(std::size_t i, std::size_t j) const;

        // The same matrix as a dense matrix-valued function
        Function dense() const;

        /*-------------------------------------- Evaluation --------------------------------------*/

        // The nonzero entries' values at one point, in CSR order (see Function::evaluate)
        template<class T = double>
        void evaluate(std::span<std::type_identity_t<T> const> inputs,
                      std::span<std::type_identity_t<T>> values) const
        {this->values_.evaluate<T>(inputs, values);}

        // The same, with the parameters' values given separately
        template<class T = double>
        void evaluate(std::span<std::type_identity_t<T> const> point,
                      std::span<std::type_identity_t<T> const> parameters,
                      std::span<std::type_identity_t<T>> values) const
        {this->values_.evaluate<T>(point, parameters, values);}
    };

    // Writes the nonzero entries as {(i, j): expression, ...}, counted from 1
    std::ostream& operator<<(std::ostream& out, SparseMatrixFunction const& matrix);


    namespace detail
    {
        // Whether an expression is identically zero
        inline bool is_zero_entry(Expr const& expr)
        {return expr.is_number() && expr.number().is_zero();}
    }


   /*-----------------------------------------------------------------------------------------------
    * SparseMatrixFunction::SparseMatrixFunction(Function const& matrix)
    *
    * Keeps the entries of a matrix-valued function that are not identically zero, row by row.
    *---------------------------------------------------------------------------------------------*/
    inline SparseMatrixFunction::SparseMatrixFunction(Function const& matrix)
        : rows_(0), columns_(0), row_offsets_{0}
    {
        if (matrix.shape().size() != 2)
        {
            throw std::invalid_argument("SparseMatrixFunction: the function's outputs must be a "
                                        "matrix, not of rank "
                                        + std::to_string(matrix.shape().size()));
        }
        this->rows_    = matrix.shape()[0];
        this->columns_ = matrix.shape()[1];

        std::vector<Expr> entries;
        for (std::size_t i = 0; i < this->rows_; ++i)
        {
            for (std::size_t j = 0; j < this->columns_; ++j)
            {
                Expr const& entry = matrix.outputs()[i * this->columns_ + j];
                if (detail::is_zero_entry(entry)) continue;
                this->column_indices_.push_back(j);
                entries.push_back(entry);
            }
            this->row_offsets_.push_back(this->column_indices_.size());
        }
        this->values_ = Function(matrix.domain(), Tensor<Expr>(std::move(entries)),
                                 Parameters(matrix.parameters()));
    }


   /*-----------------------------------------------------------------------------------------------
    * SparseMatrixFunction::SparseMatrixFunction(std::size_t rows, std::size_t columns,
    *                                            std::vector<std::size_t> row_offsets,
    *                                            std::vector<std::size_t> column_indices,
    *                                            Function values)
    *
    * A sparse matrix function from its CSR pattern and its values, after checking that they fit
    * together: rows + 1 offsets, starting at 0, never decreasing, ending at the number of
    * entries; columns inside the matrix and increasing within each row; one value per entry.
    *---------------------------------------------------------------------------------------------*/
    inline SparseMatrixFunction::SparseMatrixFunction(std::size_t rows, std::size_t columns,
                                                      std::vector<std::size_t> row_offsets,
                                                      std::vector<std::size_t> column_indices,
                                                      Function values)
        : rows_(rows), columns_(columns), row_offsets_(std::move(row_offsets)),
          column_indices_(std::move(column_indices)), values_(std::move(values))
    {
        auto fail = [](std::string const& problem)
        {throw std::invalid_argument("SparseMatrixFunction: " + problem);};

        std::vector<std::size_t> const& offsets = this->row_offsets_;
        std::vector<std::size_t> const& indices = this->column_indices_;
        if (offsets.size() != rows + 1)
        {
            fail("expected " + std::to_string(rows + 1) + " row offsets, got "
                 + std::to_string(offsets.size()));
        }
        if (offsets.front() != 0 || offsets.back() != indices.size())
            fail("row offsets must run from 0 to the number of entries");
        if (this->values_.size() != indices.size())
        {
            fail("the pattern has " + std::to_string(indices.size()) + " entries but there are "
                 + std::to_string(this->values_.size()) + " values");
        }
        for (std::size_t i = 0; i < rows; ++i)
        {
            if (offsets[i + 1] < offsets[i]) fail("row offsets decrease at row " + std::to_string(i));
            for (std::size_t p = offsets[i]; p < offsets[i + 1]; ++p)
            {
                if (indices[p] >= columns)
                    fail("column index " + std::to_string(indices[p]) + " is outside the matrix");
                if (p > offsets[i] && indices[p] <= indices[p - 1])
                    fail("column indices must increase within row " + std::to_string(i));
            }
        }
    }


   /*-----------------------------------------------------------------------------------------------
    * Expr SparseMatrixFunction::entry(std::size_t i, std::size_t j) const
    *
    * The expression of entry (i, j), counted from 1, found by binary search in row i.
    *---------------------------------------------------------------------------------------------*/
    inline Expr SparseMatrixFunction::entry(std::size_t i, std::size_t j) const
    {
        if (i < 1 || i > this->rows_ || j < 1 || j > this->columns_)
        {
            std::string size = std::to_string(this->rows_) + "x" + std::to_string(this->columns_);
            throw std::out_of_range("SparseMatrixFunction: entry (" + std::to_string(i) + ", "
                                    + std::to_string(j) + ") is outside the " + size
                                    + " matrix (indices count from 1)");
        }
        auto begin = this->column_indices_.begin();
        auto first = begin + static_cast<std::ptrdiff_t>(this->row_offsets_[i - 1]);
        auto last  = begin + static_cast<std::ptrdiff_t>(this->row_offsets_[i]);
        auto it    = std::lower_bound(first, last, j - 1);
        if (it == last || *it != j - 1) return Expr(0);
        return this->values_.outputs()[static_cast<std::size_t>(it - begin)];
    }


   /*-----------------------------------------------------------------------------------------------
    * Function SparseMatrixFunction::dense() const
    *
    * The matrix as a dense matrix-valued function, with zeros where no entry is stored.
    *---------------------------------------------------------------------------------------------*/
    inline Function SparseMatrixFunction::dense() const
    {
        Tensor<Expr> matrix(Shape{this->rows_, this->columns_});
        for (std::size_t i = 0; i < this->rows_; ++i)
            for (std::size_t p = this->row_offsets_[i]; p < this->row_offsets_[i + 1]; ++p)
                matrix[i * this->columns_ + this->column_indices_[p]] = this->values_.outputs()[p];
        return Function(this->values_.domain(), std::move(matrix),
                        Parameters(this->values_.parameters()));
    }


    inline std::ostream& operator<<(std::ostream& out, SparseMatrixFunction const& matrix)
    {
        out << "{";
        for (std::size_t i = 0; i < matrix.rows(); ++i)
        {
            for (std::size_t p = matrix.row_offsets()[i]; p < matrix.row_offsets()[i + 1]; ++p)
            {
                out << (p == 0 ? "" : ", ") << "(" << i + 1 << ", " << matrix.column_indices()[p] + 1
                    << "): " << matrix.values().outputs()[p];
            }
        }
        return out << "}";
    }


   /*-----------------------------------------------------------------------------------------------
    * SparseMatrixFunction Function::sparse_jacobian() const
    *
    * The Jacobian as a sparse matrix function: row i holds the derivatives of output i, column j
    * the derivatives with respect to the domain's variable j (in storage order). Each output is
    * differentiated only with respect to the variables that appear in it, so a large system never
    * builds its full matrix of mostly-zero derivatives; a derivative that is identically zero
    * after simplification is not stored either.
    *---------------------------------------------------------------------------------------------*/
    inline SparseMatrixFunction Function::sparse_jacobian() const
    {
        if (this->outputs_.rank() > 1)
        {
            throw std::logic_error("Function::sparse_jacobian: the outputs must be a scalar or a "
                                   "vector, not of rank " + std::to_string(this->outputs_.rank())
                                   + "; for a matrix-valued function, use SparseMatrixFunction");
        }

        // Column of each variable of the domain
        std::unordered_map<std::uint64_t, std::size_t> column_of;
        for (std::size_t j = 0; j < this->domain_.size(); ++j)
            column_of.emplace(this->domain_[j].id(), j);

        std::vector<std::size_t> row_offsets{0};
        std::vector<std::size_t> column_indices;
        std::vector<Expr>        entries;
        for (Expr const& output : this->outputs_)
        {
            std::vector<std::size_t> columns;
            for (Symbol const& variable : symbols(output))
            {
                auto it = column_of.find(variable.id());
                if (it != column_of.end()) columns.push_back(it->second);
            }
            std::sort(columns.begin(), columns.end());

            for (std::size_t j : columns)
            {
                Expr derivative = lode::diff(output, this->domain_[j]);
                if (detail::is_zero_entry(derivative)) continue;
                column_indices.push_back(j);
                entries.push_back(derivative);
            }
            row_offsets.push_back(column_indices.size());
        }

        Function values(this->domain_, Tensor<Expr>(std::move(entries)),
                        Parameters(this->parameters_));
        return SparseMatrixFunction(this->outputs_.size(), this->domain_.size(),
                                    std::move(row_offsets), std::move(column_indices),
                                    std::move(values));
    }
}
#endif
