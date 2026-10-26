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
#include <vector>

#include "lode/number.hpp"

namespace lode
{
    namespace detail
    {
        class Node;
    }

    // Kinds of expression node. The order matches the alternatives of detail::Node::Data.
    enum class Kind {Number, Symbol, Add, Mul, Pow, Call};

    // Elementary functions that can be applied to an expression
    enum class Elementary {Exp, Log, Sin, Cos};


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
        bool is_call() const {return this->kind() == Kind::Call;}

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

        // The variable an expression consists of; throws std::invalid_argument if the
        // expression is not a single symbol
        static Symbol from(Expr const& expr);

      private:
        // Wraps an expression known to be a symbol
        explicit Symbol(Expr expr) : Expr(std::move(expr)) {}
    };


    // Canonical order of expressions: the strict total order in which terms and factors are kept
    std::strong_ordering compare(Expr const& a, Expr const& b);

    // The power base^exponent
    Expr pow(Expr const& base, Expr const& exponent);

    // The square root, as the power 1/2
    Expr sqrt(Expr const& value);

    /*----------------------------------- Elementary functions -----------------------------------*/

    // The exponential e^value
    Expr exp(Expr const& value);

    // The natural logarithm; throws std::domain_error for log(0)
    Expr log(Expr const& value);

    // Sine and cosine, of an angle in radians
    Expr sin(Expr const& value);
    Expr cos(Expr const& value);

    /*-------------------------------- Calculus and substitution ---------------------------------*/

    // The derivative of an expression with respect to a variable, of the given order (at least
    // 0); throws std::invalid_argument for a negative order
    Expr diff(Expr const& expr, Symbol const& variable, int order = 1);

    // Replacements for variables: each Symbol paired with the expression that replaces it
    using Substitution = std::vector<std::pair<Symbol, Expr>>;

    // An expression with variables replaced, all at once, so {{x, y}, {y, x}} swaps x and y.
    // The result is rebuilt in canonical form, so replacing variables by numbers folds them:
    // x*y + y^2 with y = 2 gives 2*x + 4. Throws std::invalid_argument if a variable is listed
    // twice.
    Expr substitute(Expr const& expr, Substitution const& replacements);

    // The variables in an expression, in canonical order (by name)
    std::vector<Symbol> symbols(Expr const& expr);

    /*---------------------------------------- Evaluation ----------------------------------------*/

    // Values for variables: each Symbol paired with its value
    template<class T>
    using Values = std::vector<std::pair<Symbol, T>>;

    // Numeric value of an expression for given values of its variables, computed in type T
    // (double unless given; std::complex<double> also works). A reference implementation that
    // walks the expression, meant for checking results rather than for speed.
    template<class T = double>
    T evaluate(Expr const& expr, Values<T> const& values);

    /*------------------------------------------ Output ------------------------------------------*/

    // Writes the expression's text form
    std::ostream& operator<<(std::ostream& out, Expr const& expr);
}

// Implementation, in dependency order
#include "lode/detail/node.hpp"
#include "lode/detail/canonical.hpp"
#include "lode/detail/print.hpp"
#include "lode/detail/expr_impl.hpp"
#include "lode/detail/calculus.hpp"
#include "lode/detail/evaluate.hpp"

#endif
