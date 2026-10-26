/*--------------------------------------------------------------------------------------------------
 * File: print.hpp
 *
 * Description: Text form of expressions. Included by expr.hpp; not meant to be included on its
 *              own.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_DETAIL_PRINT_HPP
#define LODE_DETAIL_PRINT_HPP

#include <cstddef>
#include <string>
#include <vector>

namespace lode::detail
{
    // How tightly a printed form binds, loosest first. An operand that binds more loosely than
    // its context requires is put in parentheses.
    enum class Precedence {Sum, Product, Power, Atom};

    std::string print(Expr const& expr);


   /*-----------------------------------------------------------------------------------------------
    * Precedence precedence(Expr const& expr)
    *
    * How tightly the printed form of an expression binds. A negative or fractional number binds
    * like a product ("-2", "1/2"), and so does a power with a negative exponent ("1/x").
    *---------------------------------------------------------------------------------------------*/
    inline Precedence precedence(Expr const& expr)
    {
        switch (expr.kind())
        {
            case Kind::Number:
            {
                Number const& n = expr.number();
                bool fraction = n.is_rational() && !n.is_integer();
                return n.is_negative() || fraction ? Precedence::Product : Precedence::Atom;
            }
            case Kind::Symbol:
            case Kind::Call:
                return Precedence::Atom;
            case Kind::Add:
                return Precedence::Sum;
            case Kind::Mul:
                return Precedence::Product;
            case Kind::Pow:
            {
                Expr const& exponent = expr.node().pow().exponent;
                bool inverse = exponent.is_number() && exponent.number().is_negative();
                return inverse ? Precedence::Product : Precedence::Power;
            }
        }
        return Precedence::Atom;
    }


    // Prints an expression, in parentheses if it binds more loosely than the context requires
    inline std::string print_operand(Expr const& expr, Precedence context)
    {
        std::string text = print(expr);
        return precedence(expr) < context ? "(" + text + ")" : text;
    }

    // Name of an elementary function, as printed
    inline char const* function_name(Elementary function)
    {
        switch (function)
        {
            case Elementary::Exp: return "exp";
            case Elementary::Log: return "log";
            case Elementary::Sin: return "sin";
            case Elementary::Cos: return "cos";
        }
        return "?";
    }

    // Joins strings with a separator
    inline std::string join(std::vector<std::string> const& parts, std::string const& separator)
    {
        std::string text;
        for (std::size_t i = 0; i < parts.size(); ++i)
            text += (i == 0 ? "" : separator) + parts[i];
        return text;
    }

    // The factors of an expression seen as a product: a product's own factors, a power's base
    // and exponent, or the expression itself to the power 1
    inline std::vector<Factor> factors_of(Expr const& expr)
    {
        if (expr.is_mul()) return expr.node().mul().factors;
        if (expr.is_pow()) return {Factor{expr.node().pow().base, expr.node().pow().exponent}};
        return {Factor{expr, one()}};
    }


   /*-----------------------------------------------------------------------------------------------
    * std::string print_power(Expr const& base, Expr const& exponent)
    *
    * Prints one factor of a product, base^exponent, as "x", "x^2", "(x + y)^2" or "x^(1/2)".
    * The exponent is not a negative number: those factors go in the denominator.
    *---------------------------------------------------------------------------------------------*/
    inline std::string print_power(Expr const& base, Expr const& exponent)
    {
        if (exponent.is_number() && exponent.number().is_one())
            return print_operand(base, Precedence::Power);
        return print_operand(base, Precedence::Atom) + "^" + print_operand(exponent, Precedence::Atom);
    }


   /*-----------------------------------------------------------------------------------------------
    * std::string print_product(Number const& coefficient, std::vector<Factor> const& factors)
    *
    * Prints coefficient times a product of factors as a quotient: factors with negative numeric
    * exponents, and a rational coefficient's denominator, go below the line ("-3*x/(4*y)").
    *---------------------------------------------------------------------------------------------*/
    inline std::string print_product(Number const& coefficient, std::vector<Factor> const& factors)
    {
        std::vector<std::string> numerator;
        std::vector<std::string> denominator;

        Number magnitude = coefficient.is_negative() ? -coefficient : coefficient;
        if (magnitude.is_real())
            numerator.push_back(magnitude.to_string());
        else
        {
            if (magnitude.numerator() != 1)
                numerator.push_back(std::to_string(magnitude.numerator()));
            if (magnitude.denominator() != 1)
                denominator.push_back(std::to_string(magnitude.denominator()));
        }

        for (Factor const& factor : factors)
        {
            bool inverse = factor.exponent.is_number() && factor.exponent.number().is_negative();
            if (inverse)
                denominator.push_back(print_power(factor.base, -factor.exponent));
            else
                numerator.push_back(print_power(factor.base, factor.exponent));
        }

        std::string text = coefficient.is_negative() ? "-" : "";
        text += numerator.empty() ? "1" : join(numerator, "*");
        if (denominator.size() == 1) text += "/" + denominator[0];
        if (denominator.size() > 1) text += "/(" + join(denominator, "*") + ")";
        return text;
    }


    // Appends a term to a sum's text, with its sign as the operator ("x - y", not "x + -y")
    inline void append_signed(std::string& text, bool negative, std::string const& magnitude)
    {
        if (text.empty())
            text = negative ? "-" + magnitude : magnitude;
        else
            text += (negative ? " - " : " + ") + magnitude;
    }


   /*-----------------------------------------------------------------------------------------------
    * std::string print(Expr const& expr)
    *
    * Text form of an expression. A sum lists its terms in canonical order, then its constant.
    *---------------------------------------------------------------------------------------------*/
    inline std::string print(Expr const& expr)
    {
        switch (expr.kind())
        {
            case Kind::Number:
                return expr.number().to_string();

            case Kind::Symbol:
                return expr.node().symbol().name;

            case Kind::Mul:
                return print_product(expr.node().mul().coefficient, expr.node().mul().factors);

            case Kind::Pow:
                return print_product(Number(1), factors_of(expr));

            case Kind::Call:
            {
                CallData const& call = expr.node().call();
                return std::string(function_name(call.function)) + "(" + print(call.argument) + ")";
            }

            case Kind::Add:
            {
                AddData const& sum = expr.node().add();
                std::string text;
                for (Term const& term : sum.terms)
                {
                    bool   negative  = term.coefficient.is_negative();
                    Number magnitude = negative ? -term.coefficient : term.coefficient;
                    append_signed(text, negative, print_product(magnitude, factors_of(term.expr)));
                }
                if (!sum.constant.is_zero())
                {
                    bool   negative  = sum.constant.is_negative();
                    Number magnitude = negative ? -sum.constant : sum.constant;
                    append_signed(text, negative, magnitude.to_string());
                }
                return text;
            }
        }
        return "";
    }
}
#endif
