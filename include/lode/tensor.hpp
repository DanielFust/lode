/*--------------------------------------------------------------------------------------------------
 * File: tensor.hpp
 *
 * Description: Tensors: arrays of any rank, stored contiguously, used for a function's outputs
 *              (expressions), its variables (symbols) and its values (numbers).
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_TENSOR_HPP
#define LODE_TENSOR_HPP

#include <array>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "lode/expr.hpp"

namespace lode
{
   /*-----------------------------------------------------------------------------------------------
    * class Shape
    *
    * The dimensions of a tensor, given explicitly: Tensor<double> t(Shape{2, 3}, values). A
    * separate type, so that a list of dimensions is never mistaken for a list of elements.
    *---------------------------------------------------------------------------------------------*/
    class Shape
    {
      private:
        std::vector<std::size_t> dimensions_;

      public:
        // Implicit, so that a braced list of dimensions can be given where a Shape is expected
        Shape(std::initializer_list<std::size_t> dimensions) : dimensions_(dimensions) {}

        explicit Shape(std::vector<std::size_t> dimensions) : dimensions_(std::move(dimensions)) {}

        std::vector<std::size_t> const& dimensions() const {return this->dimensions_;}
    };


   /*-----------------------------------------------------------------------------------------------
    * class Tensor<T>
    *
    * An array of any rank, its elements stored contiguously in row-major order (the last index
    * varies fastest). A tensor of rank 0 holds one element; rank 1 is a vector, rank 2 a matrix.
    *
    * Two ways to index it, following the convention that the bracket shows the base:
    *
    *     t(i, j, ...)    the mathematical entry, one index per dimension, each counted from 1
    *                     (checked: a wrong count or an index out of range throws)
    *     t[k]            the k-th element of storage, counted from 0 (not checked, as for
    *                     std::vector)
    *
    * Construction follows std::vector: parentheses give dimensions, braces give elements.
    *
    *     Tensor<double> a(2, 3);              2x3, elements value-initialized (zero)
    *     Tensor<Expr>   v{x, y, z};           a vector of three expressions
    *     Tensor<Expr>   m{{a, b}, {c, d}};    a 2x2 matrix, row by row
    *     Tensor<double> t(Shape{2, 2, 2}, values);   from a shape and row-major elements
    *
    * Template Parameters:
    *     T         Element type
    *---------------------------------------------------------------------------------------------*/
    template<class T>
    class Tensor
    {
      private:
        std::vector<std::size_t> shape_;    // Dimensions; empty for rank 0
        std::vector<T>           elements_; // Row-major

        // Indices given to operator(), widened so that negative ones can be reported
        template<std::size_t N>
        using Indices = std::array<long long, N>;

      public:
        /*------------------------------------- Construction -------------------------------------*/

        // A rank-0 tensor of one value-initialized element
        Tensor() requires std::default_initializable<T> : elements_(1) {}

        // A tensor of the given dimensions, elements value-initialized: Tensor<double> a(2, 3)
        template<std::integral... I>
            requires (sizeof...(I) > 0 && std::default_initializable<T>)
        explicit Tensor(I... dimensions)
            : shape_{static_cast<std::size_t>(dimensions)...}, elements_(count(this->shape_)) {}

        // A tensor of the given shape, elements value-initialized
        explicit Tensor(Shape const& shape) requires std::default_initializable<T>
            : shape_(shape.dimensions()), elements_(count(this->shape_)) {}

        // A tensor of the given shape from its elements in row-major order; throws
        // std::invalid_argument if their number does not match the shape
        Tensor(Shape const& shape, std::vector<T> elements);

        // A vector of the given elements. Implicit, so that a std::vector can be passed where a
        // tensor is expected.
        Tensor(std::vector<T> elements) : shape_{elements.size()}, elements_(std::move(elements)) {}

        // A vector, from a braced list: {x, y, z}
        Tensor(std::initializer_list<T> elements) : shape_{elements.size()}, elements_(elements) {}

        // A matrix, from a braced list of rows: {{a, b}, {c, d}}; throws std::invalid_argument
        // if the rows differ in length
        Tensor(std::initializer_list<std::initializer_list<T>> rows);

        /*--------------------------------------- Queries ----------------------------------------*/

        std::size_t rank() const {return this->shape_.size();}
        std::vector<std::size_t> const& shape() const {return this->shape_;}

        // Number of elements
        std::size_t size() const {return this->elements_.size();}

        // Storage, in row-major order
        T* data() {return this->elements_.data();}
        T const* data() const {return this->elements_.data();}
        auto begin() {return this->elements_.begin();}
        auto end() {return this->elements_.end();}
        auto begin() const {return this->elements_.begin();}
        auto end() const {return this->elements_.end();}

        // The elements, in row-major order
        std::vector<T> const& elements() const {return this->elements_;}

        /*--------------------------------------- Indexing ---------------------------------------*/

        // The entry with the given indices, counted from 1; throws std::out_of_range for the
        // wrong number of indices or an index out of range
        template<std::integral... I>
        T& operator()(I... indices) {return this->elements_[this->position(indices...)];}

        template<std::integral... I>
        T const& operator()(I... indices) const {return this->elements_[this->position(indices...)];}

        // The k-th element of storage, counted from 0 (not checked)
        T& operator[](std::size_t k) {return this->elements_[k];}
        T const& operator[](std::size_t k) const {return this->elements_[k];}

        /*-------------------------------------- Comparison --------------------------------------*/

        // Same shape and equal elements
        friend bool operator==(Tensor const& a, Tensor const& b) requires std::equality_comparable<T>
        {return a.shape_ == b.shape_ && a.elements_ == b.elements_;}

      private:
        // Number of elements of a shape
        static std::size_t count(std::vector<std::size_t> const& shape)
        {
            std::size_t n = 1;
            for (std::size_t dimension : shape) n *= dimension;
            return n;
        }

        // Storage position of an entry given by 1-based indices
        template<std::integral... I>
        std::size_t position(I... indices) const
        {return this->offset(Indices<sizeof...(I)>{static_cast<long long>(indices)...});}

        template<std::size_t N>
        std::size_t offset(std::array<long long, N> const& indices) const;
    };


   /*-----------------------------------------------------------------------------------------------
    * Tensor<T>::Tensor(Shape const& shape, std::vector<T> elements)
    *
    * A tensor from its shape and its elements in row-major order.
    *---------------------------------------------------------------------------------------------*/
    template<class T>
    Tensor<T>::Tensor(Shape const& shape, std::vector<T> elements)
        : shape_(shape.dimensions()), elements_(std::move(elements))
    {
        if (this->elements_.size() != count(this->shape_))
        {
            throw std::invalid_argument("Tensor: the shape holds "
                                        + std::to_string(count(this->shape_)) + " elements, but "
                                        + std::to_string(this->elements_.size()) + " were given");
        }
    }


   /*-----------------------------------------------------------------------------------------------
    * Tensor<T>::Tensor(std::initializer_list<std::initializer_list<T>> rows)
    *
    * A matrix from its rows.
    *---------------------------------------------------------------------------------------------*/
    template<class T>
    Tensor<T>::Tensor(std::initializer_list<std::initializer_list<T>> rows)
    {
        std::size_t columns = rows.size() == 0 ? 0 : rows.begin()->size();
        for (std::initializer_list<T> const& row : rows)
        {
            if (row.size() != columns)
            {
                throw std::invalid_argument("Tensor: rows of a matrix must have the same length, not "
                                            + std::to_string(columns) + " and "
                                            + std::to_string(row.size()));
            }
            this->elements_.insert(this->elements_.end(), row.begin(), row.end());
        }
        this->shape_ = {rows.size(), columns};
    }


   /*-----------------------------------------------------------------------------------------------
    * std::size_t Tensor<T>::offset(std::array<long long, N> const& indices) const
    *
    * The storage position of the entry at the given indices, counted from 1, in row-major order.
    * Throws std::out_of_range, naming the problem, for the wrong number of indices or an index
    * outside 1 to its dimension.
    *---------------------------------------------------------------------------------------------*/
    template<class T>
    template<std::size_t N>
    std::size_t Tensor<T>::offset(std::array<long long, N> const& indices) const
    {
        if (indices.size() != this->shape_.size())
        {
            std::string count = std::to_string(indices.size())
                              + (indices.size() == 1 ? " index" : " indices");
            throw std::out_of_range("Tensor: " + count + " given to a tensor of rank "
                                    + std::to_string(this->shape_.size()));
        }
        std::size_t position = 0;
        for (std::size_t d = 0; d < indices.size(); ++d)
        {
            long long index = indices[d];
            if (index < 1 || static_cast<std::size_t>(index) > this->shape_[d])
            {
                throw std::out_of_range("Tensor: index " + std::to_string(index) + " of dimension "
                                        + std::to_string(d + 1) + " is outside 1 to "
                                        + std::to_string(this->shape_[d]) + " (indices count from 1)");
            }
            position = position * this->shape_[d] + static_cast<std::size_t>(index - 1);
        }
        return position;
    }


   /*-----------------------------------------------------------------------------------------------
    * std::ostream& operator<<(std::ostream& out, Tensor<T> const& tensor)
    *
    * Writes a tensor as nested brackets, one level per dimension: "[x, y]", "[[1, 0], [0, 1]]".
    * A rank-0 tensor is written as its element.
    *---------------------------------------------------------------------------------------------*/
    template<class T>
    std::ostream& operator<<(std::ostream& out, Tensor<T> const& tensor)
    {
        std::vector<std::size_t> const& shape = tensor.shape();

        // Writes the block starting at element `first`, spanning dimensions d onward
        auto write = [&](auto const& self, std::size_t first, std::size_t d) -> void
        {
            if (d == shape.size())
            {
                out << tensor[first];
                return;
            }
            std::size_t stride = 1;
            for (std::size_t e = d + 1; e < shape.size(); ++e) stride *= shape[e];
            out << "[";
            for (std::size_t i = 0; i < shape[d]; ++i)
            {
                if (i > 0) out << ", ";
                self(self, first + i * stride, d + 1);
            }
            out << "]";
        };
        write(write, 0, 0);
        return out;
    }


   /*-----------------------------------------------------------------------------------------------
    * Tensor<Symbol> make_symbols(std::string const& name, std::vector<std::size_t> const& shape)
    *
    * A tensor of new variables, named after the tensor with their indices from 1: for name F and
    * shape {2, 2}, the variables F11, F12, F21, F22. If a dimension exceeds 9, the indices are
    * separated by underscores (F_10_1) so that names stay unambiguous.
    *
    * Parameters:
    *     name     Base name of the variables
    *     shape    Dimensions of the tensor
    *
    * Returns:
    *     The variables, as a tensor of the given shape
    *---------------------------------------------------------------------------------------------*/
    inline Tensor<Symbol> make_symbols(std::string const& name, std::vector<std::size_t> const& shape)
    {
        bool separate = false;
        std::size_t total = 1;
        for (std::size_t dimension : shape)
        {
            separate = separate || dimension > 9;
            total *= dimension;
        }

        std::vector<Symbol> variables;
        variables.reserve(total);
        std::vector<std::size_t> index(shape.size(), 1);
        for (std::size_t k = 0; k < total; ++k)
        {
            std::string variable = name;
            for (std::size_t i : index) variable += (separate ? "_" : "") + std::to_string(i);
            variables.emplace_back(variable);

            // Next index in row-major order
            for (std::size_t d = shape.size(); d-- > 0;)
            {
                if (++index[d] <= shape[d]) break;
                index[d] = 1;
            }
        }
        return Tensor<Symbol>(Shape(shape), std::move(variables));
    }
}
#endif
