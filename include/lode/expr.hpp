/*--------------------------------------------------------------------------------------------------
 * File: expr.hpp
 *
 * Description: Symbolic expressions (Expr) and variables (Symbol), with the arithmetic that builds
 *              expressions in canonical form.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_EXPR_HPP
#define LODE_EXPR_HPP

#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <utility>

#include "lode/number.hpp"

namespace lode
{
    namespace detail
    {
        class Node;
    }

    // Kinds of expression node. The order matches the alternatives of detail::Node::Data.
    enum class Kind {Number, Symbol, Add, Mul, Pow};


   /*-----------------------------------------------------------------------------------------------
    * class Expr
    *
    * A symbolic expression. An Expr is a small value that shares an immutable node, so copying
    * one is cheap and an expression never changes once built.
    *
    * Every expression is kept in canonical form. Building one with the arithmetic operators
    * flattens nested sums and products, combines like terms (x + x = 2*x) and like factors
    * (x*x = x^2), folds numbers, and sorts the result into a fixed order. Expressions that differ
    * only in how their terms are ordered or grouped are therefore equal. Products of sums are not
    * expanded: x*(y + z) stays as it is.
    *---------------------------------------------------------------------------------------------*/
    class Expr
    {
      private:
        std::shared_ptr<detail::Node const> node_;

      public:
        /*------------------------------------- Constructors -------------------------------------*/

        // Zero
        Expr();

        // A number. Implicit, as are the next two constructors, so that numbers can be used
        // directly in expressions such as 2*x + 1.
        Expr(Number const& value);

        // An exact integer
        template<std::integral I>
            requires (!std::same_as<I, bool>)
        Expr(I value) : Expr(Number(value)) {}

        // A real
        template<std::floating_point F>
        Expr(F value) : Expr(Number(value)) {}

        // Wraps a node built by the library's internals
        explicit Expr(std::shared_ptr<detail::Node const> node) : node_(std::move(node)) {}

        /*--------------------------------------- Queries ----------------------------------------*/

        // Kind of the top-level node
        Kind kind() const;

        bool is_number() const {return this->kind() == Kind::Number;}
        bool is_symbol() const {return this->kind() == Kind::Symbol;}
        bool is_add() const {return this->kind() == Kind::Add;}
        bool is_mul() const {return this->kind() == Kind::Mul;}
        bool is_pow() const {return this->kind() == Kind::Pow;}

        // Value of a number; throws std::logic_error for any other expression
        Number const& number() const;

        // Hash; equal expressions have equal hashes
        std::size_t hash() const;

        // Text form, such as "x + 2*x^2 - 1"
        std::string to_string() const;

        // Underlying node, for the library's internals
        detail::Node const& node() const {return *this->node_;}

        /*-------------------------------------- Arithmetic --------------------------------------*/

        friend Expr operator+(Expr const& a, Expr const& b);
        friend Expr operator-(Expr const& a, Expr const& b);
        friend Expr operator*(Expr const& a, Expr const& b);
        friend Expr operator/(Expr const& a, Expr const& b);
        friend Expr operator-(Expr const& a);

        Expr& operator+=(Expr const& b) {return *this = *this + b;}
        Expr& operator-=(Expr const& b) {return *this = *this - b;}
        Expr& operator*=(Expr const& b) {return *this = *this * b;}
        Expr& operator/=(Expr const& b) {return *this = *this / b;}

        /*-------------------------------------- Comparison --------------------------------------*/

        // Structural equality of the canonical forms
        friend bool operator==(Expr const& a, Expr const& b);
    };


   /*-----------------------------------------------------------------------------------------------
    * class Symbol
    *
    * A variable. Every Symbol constructed is a new variable, distinct from all others even if it
    * has the same name; copies of a Symbol are the same variable. A Symbol is an Expr, so it can
    * be used wherever an expression can.
    *---------------------------------------------------------------------------------------------*/
    class Symbol : public Expr
    {
      public:
        // A new variable; throws std::invalid_argument if the name is empty
        explicit Symbol(std::string name);

        // Name, used for printing
        std::string const& name() const;

        // Identifier that distinguishes this variable from others of the same name
        std::uint64_t id() const;
    };


    // Canonical order of expressions: the strict total order in which terms and factors are kept
    std::strong_ordering compare(Expr const& a, Expr const& b);

    // The power base^exponent
    Expr pow(Expr const& base, Expr const& exponent);

    // The square root, as the power 1/2
    Expr sqrt(Expr const& value);

    // Writes the expression's text form
    std::ostream& operator<<(std::ostream& out, Expr const& expr);
}

// Implementation, in dependency order
#include "lode/detail/node.hpp"
#include "lode/detail/canonical.hpp"
#include "lode/detail/print.hpp"
#include "lode/detail/expr_impl.hpp"

#endif
