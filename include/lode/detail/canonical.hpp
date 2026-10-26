/*--------------------------------------------------------------------------------------------------
 * File: canonical.hpp
 *
 * Description: The canonical order of expressions, and the construction of sums, products,
 *              powers and elementary function calls in canonical form. Included by expr.hpp;
 *              not meant to be included on its own.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_DETAIL_CANONICAL_HPP
#define LODE_DETAIL_CANONICAL_HPP

#include <algorithm>
#include <cmath>
#include <compare>
#include <cstddef>
#include <map>
#include <stdexcept>
#include <optional>
#include <utility>
#include <vector>

namespace lode
{
    namespace detail
    {
        // Rank of each kind in the canonical order. Powers have no rank of their own: they sort
        // by their base, next to it.
        inline int kind_rank(Kind kind)
        {
            switch (kind)
            {
                case Kind::Number: return 0;
                case Kind::Symbol: return 1;
                case Kind::Call:   return 2;
                case Kind::Mul:    return 3;
                case Kind::Add:    return 4;
                case Kind::Pow:    return 5;
            }
            return 5;
        }
    }


   /*-----------------------------------------------------------------------------------------------
    * std::strong_ordering compare(Expr const& a, Expr const& b)
    *
    * The canonical order: a strict total order on expressions, equal only for equal expressions.
    *
    * A power sorts by its base, then its exponent, and any other expression e counts as e^1
    * here, so x, x^2 and x^3 sort together, before y. Otherwise numbers come first (by value),
    * then symbols (by name), then function calls (by function, then argument), then products,
    * then sums. Products and sums compare their parts in order.
    *---------------------------------------------------------------------------------------------*/
    inline std::strong_ordering compare(Expr const& a, Expr const& b)
    {
        if (&a.node() == &b.node()) return std::strong_ordering::equal;

        if (a.is_pow() || b.is_pow())
        {
            Expr const& base_a     = a.is_pow() ? a.node().pow().base : a;
            Expr const& base_b     = b.is_pow() ? b.node().pow().base : b;
            Expr const& exponent_a = a.is_pow() ? a.node().pow().exponent : detail::one();
            Expr const& exponent_b = b.is_pow() ? b.node().pow().exponent : detail::one();
            std::strong_ordering order = compare(base_a, base_b);
            if (std::is_neq(order)) return order;
            return compare(exponent_a, exponent_b);
        }

        if (a.kind() != b.kind()) return detail::kind_rank(a.kind()) <=> detail::kind_rank(b.kind());

        switch (a.kind())
        {
            case Kind::Number:
                return compare(a.number(), b.number());

            case Kind::Symbol:
            {
                detail::SymbolData const& x = a.node().symbol();
                detail::SymbolData const& y = b.node().symbol();
                std::strong_ordering order = x.name <=> y.name;
                if (std::is_neq(order)) return order;
                return x.id <=> y.id;
            }

            case Kind::Mul:
            {
                detail::MulData const& x = a.node().mul();
                detail::MulData const& y = b.node().mul();
                std::size_t n = std::min(x.factors.size(), y.factors.size());
                for (std::size_t i = 0; i < n; ++i)
                {
                    std::strong_ordering order = compare(x.factors[i].base, y.factors[i].base);
                    if (std::is_neq(order)) return order;
                    order = compare(x.factors[i].exponent, y.factors[i].exponent);
                    if (std::is_neq(order)) return order;
                }
                std::strong_ordering order = x.factors.size() <=> y.factors.size();
                if (std::is_neq(order)) return order;
                return compare(x.coefficient, y.coefficient);
            }

            case Kind::Add:
            {
                detail::AddData const& x = a.node().add();
                detail::AddData const& y = b.node().add();
                std::size_t n = std::min(x.terms.size(), y.terms.size());
                for (std::size_t i = 0; i < n; ++i)
                {
                    std::strong_ordering order = compare(x.terms[i].expr, y.terms[i].expr);
                    if (std::is_neq(order)) return order;
                    order = compare(x.terms[i].coefficient, y.terms[i].coefficient);
                    if (std::is_neq(order)) return order;
                }
                std::strong_ordering order = x.terms.size() <=> y.terms.size();
                if (std::is_neq(order)) return order;
                return compare(x.constant, y.constant);
            }

            case Kind::Call:
            {
                detail::CallData const& x = a.node().call();
                detail::CallData const& y = b.node().call();
                std::strong_ordering order = x.function <=> y.function;
                if (std::is_neq(order)) return order;
                return compare(x.argument, y.argument);
            }

            case Kind::Pow:
                break;
        }
        return std::strong_ordering::equal;
    }


    namespace detail
    {
        // Comparator for ordered containers of expressions
        struct ExprLess
        {
            bool operator()(Expr const& a, Expr const& b) const
            {return std::is_lt(lode::compare(a, b));}
        };

        // Coefficients of a sum's terms, keyed by term
        using TermMap = std::map<Expr, Number, ExprLess>;

        // Exponents of a product's factors, keyed by base
        using PowerMap = std::map<Expr, Expr, ExprLess>;

        Expr make_add(std::vector<Expr> const& operands);
        Expr make_mul(std::vector<Expr> const& operands);
        Expr make_pow(Expr const& base, Expr const& exponent);
        Expr make_call(Elementary function, Expr const& argument);

        // Whether an expression is an exponential, exp(u)
        inline bool is_exp(Expr const& expr)
        {return expr.is_call() && expr.node().call().function == Elementary::Exp;}


        // Adds coefficient*expr to the terms of a sum
        inline void accumulate_term(TermMap& terms, Expr const& expr, Number const& coefficient)
        {
            auto [it, inserted] = terms.try_emplace(expr, coefficient);
            if (!inserted) it->second += coefficient;
        }

        // Multiplies base^exponent into the factors of a product
        inline void accumulate_power(PowerMap& powers, Expr const& base, Expr const& exponent)
        {
            auto [it, inserted] = powers.try_emplace(base, exponent);
            if (!inserted) it->second = make_add({it->second, exponent});
        }

        // A product with its coefficient replaced by 1
        inline Expr without_coefficient(MulData const& product)
        {
            if (product.factors.size() == 1)
                return make_pow(product.factors[0].base, product.factors[0].exponent);
            return make_expr(MulData{Number(1), product.factors});
        }


       /*-------------------------------------------------------------------------------------------
        * std::pair<Number, Expr> split_content(Expr const& sum)
        *
        * Splits a sum into its content and a primitive sum, so that sum = content*primitive.
        *
        * The content is the largest rational that divides the constant and every coefficient to
        * an integer, signed so that the primitive sum's first term is positive: 2*x + 2*y splits
        * as 2*(x + y), and -x - y as -1*(x + y). A sum stands in a product only in primitive
        * form, which makes products of sums canonical: (2*x + 2*y)*z and 2*((x + y)*z) are both
        * 2*z*(x + y).
        *
        * A sum with a real coefficient or constant is left whole, with content 1, as is one whose
        * content would overflow 64 bits.
        *
        * Parameters:
        *     sum    A sum (an expression of kind Add)
        *
        * Returns:
        *     The content and the primitive sum
        *-----------------------------------------------------------------------------------------*/
        inline std::pair<Number, Expr> split_content(Expr const& sum)
        {
            AddData const& data = sum.node().add();

            // Content magnitude: gcd of the numerators over lcm of the denominators
            Int128 numerator_gcd   = 0;
            Int128 denominator_lcm = 1;
            bool   all_rational    = true;
            auto include = [&](Number const& n)
            {
                if (n.is_real())
                {
                    all_rational = false;
                    return;
                }
                Int128 numerator = n.numerator();
                numerator_gcd    = gcd(numerator_gcd, numerator < 0 ? -numerator : numerator);
                denominator_lcm  = denominator_lcm / gcd(denominator_lcm, n.denominator())
                                 * n.denominator();
            };
            include(data.constant);
            for (Term const& term : data.terms)
            {
                include(term.coefficient);
                if (!all_rational || !fits_int64(denominator_lcm)) return {Number(1), sum};
            }
            if (!all_rational) return {Number(1), sum};

            Number content(static_cast<std::int64_t>(numerator_gcd),
                           static_cast<std::int64_t>(denominator_lcm));
            if (data.terms.front().coefficient.is_negative()) content = -content;
            if (content.is_one()) return {content, sum};

            // Dividing every coefficient by the content keeps their order and nonzero values
            std::vector<Term> terms;
            for (Term const& term : data.terms)
                terms.push_back(Term{term.expr, term.coefficient / content});
            return {content, make_expr(AddData{data.constant / content, std::move(terms)})};
        }


       /*-------------------------------------------------------------------------------------------
        * void add_term(Expr const& expr, Number const& scale, Number& constant, TermMap& terms)
        *
        * Adds scale*expr to a sum under construction, flattening nested sums and splitting the
        * coefficient off a product so that 2*x and 3*x collect as like terms.
        *
        * Parameters:
        *     expr        Expression to add (in canonical form)
        *     scale       Factor to multiply it by
        *     constant    The sum's constant, updated
        *     terms       The sum's terms, updated
        *-----------------------------------------------------------------------------------------*/
        inline void add_term(Expr const& expr, Number const& scale, Number& constant, TermMap& terms)
        {
            switch (expr.kind())
            {
                case Kind::Number:
                    constant += scale * expr.number();
                    return;

                case Kind::Add:
                {
                    AddData const& sum = expr.node().add();
                    constant += scale * sum.constant;
                    for (Term const& term : sum.terms)
                        accumulate_term(terms, term.expr, scale * term.coefficient);
                    return;
                }

                case Kind::Mul:
                {
                    MulData const& product = expr.node().mul();
                    if (!product.coefficient.is_one())
                    {
                        Expr rest = without_coefficient(product);
                        accumulate_term(terms, rest, scale * product.coefficient);
                        return;
                    }
                    break;
                }

                default:
                    break;
            }
            accumulate_term(terms, expr, scale);
        }


       /*-------------------------------------------------------------------------------------------
        * Expr build_add(Number constant, TermMap const& terms)
        *
        * Builds the canonical sum of a constant and collected terms, dropping terms whose
        * coefficients cancelled to zero.
        *
        * Returns:
        *     The constant if no terms remain, a product if one term remains with no constant,
        *     and otherwise a sum
        *-----------------------------------------------------------------------------------------*/
        inline Expr build_add(Number constant, TermMap const& terms)
        {
            std::vector<Term> kept;
            for (auto const& [expr, coefficient] : terms)
                if (!coefficient.is_zero()) kept.push_back(Term{expr, coefficient});

            if (kept.empty()) return Expr(constant);

            // A zero constant alongside terms is always the exact 0, so x + 0.0 equals x
            if (constant.is_zero()) constant = Number(0);
            if (kept.size() == 1 && constant.is_zero())
                return make_mul({Expr(kept[0].coefficient), kept[0].expr});
            return make_expr(AddData{constant, std::move(kept)});
        }


       /*-------------------------------------------------------------------------------------------
        * Expr make_add(std::vector<Expr> const& operands)
        *
        * The canonical sum of expressions in canonical form.
        *-----------------------------------------------------------------------------------------*/
        inline Expr make_add(std::vector<Expr> const& operands)
        {
            Number  constant = 0;
            TermMap terms;
            for (Expr const& operand : operands) add_term(operand, Number(1), constant, terms);
            return build_add(constant, terms);
        }


       /*-------------------------------------------------------------------------------------------
        * void multiply_factor(Expr const& expr, Number& coefficient, PowerMap& powers,
        *                      std::vector<Expr>& exponents)
        *
        * Multiplies an expression into a product under construction, flattening nested products
        * and collecting powers of the same base. Exponentials are set aside, to be combined into
        * one: exp(a)*exp(b) = exp(a + b).
        *
        * Parameters:
        *     expr           Expression to multiply in (in canonical form)
        *     coefficient    The product's coefficient, updated
        *     powers         The product's factors, updated
        *     exponents      Arguments of the product's exponentials, updated
        *-----------------------------------------------------------------------------------------*/
        inline void multiply_factor(Expr const& expr, Number& coefficient, PowerMap& powers,
                                    std::vector<Expr>& exponents)
        {
            if (is_exp(expr))
            {
                exponents.push_back(expr.node().call().argument);
                return;
            }

            switch (expr.kind())
            {
                case Kind::Number:
                    coefficient *= expr.number();
                    return;

                case Kind::Mul:
                {
                    MulData const& product = expr.node().mul();
                    coefficient *= product.coefficient;
                    for (Factor const& factor : product.factors)
                    {
                        if (is_exp(factor.base))
                            exponents.push_back(factor.base.node().call().argument);
                        else
                            accumulate_power(powers, factor.base, factor.exponent);
                    }
                    return;
                }

                case Kind::Pow:
                    accumulate_power(powers, expr.node().pow().base, expr.node().pow().exponent);
                    return;

                case Kind::Add:
                {
                    // A sum enters a product in primitive form, its content joining the coefficient
                    auto [content, primitive] = split_content(expr);
                    coefficient *= content;
                    accumulate_power(powers, primitive, one());
                    return;
                }

                default:
                    accumulate_power(powers, expr, one());
                    return;
            }
        }


       /*-------------------------------------------------------------------------------------------
        * Expr build_mul(Number const& coefficient, PowerMap const& powers)
        *
        * Builds the canonical product of a coefficient and collected powers.
        *
        * Each power is rebuilt with make_pow, since a combined exponent can simplify it:
        * 2^(1/2)*2^(1/2) gives 2^1 = 2, which joins the coefficient. If rebuilding changes a
        * power's base, as (x^2)^(1/2)*(x^2)^(1/2) gives x^2, the product is formed again from
        * the rebuilt powers.
        *
        * Parameters:
        *     coefficient    Product of the numeric factors
        *     powers         Exponent of each base
        *
        * Returns:
        *     The canonical product, which may be a number, a power, a sum (when a number
        *     multiplies a single sum) or a product
        *-----------------------------------------------------------------------------------------*/
        inline Expr build_mul(Number const& coefficient, PowerMap const& powers)
        {
            Number              folded = coefficient;
            std::vector<Expr>   rebuilt;
            std::vector<Factor> factors;
            std::vector<Expr>   factor_powers;
            bool                regroup = false;
            for (auto const& [base, exponent] : powers)
            {
                Expr power = make_pow(base, exponent);
                rebuilt.push_back(power);
                if (power.is_number())
                    folded *= power.number();
                else if (power.is_pow() && power.node().pow().base == base)
                {
                    factors.push_back(Factor{base, power.node().pow().exponent});
                    factor_powers.push_back(power);
                }
                else if (power == base && (base.is_symbol() || base.is_add() || base.is_call()))
                {
                    factors.push_back(Factor{base, one()});
                    factor_powers.push_back(power);
                }
                else
                    regroup = true;
            }

            if (regroup)
            {
                rebuilt.push_back(Expr(coefficient));
                return make_mul(rebuilt);
            }

            if (folded.is_zero() || factors.empty()) return Expr(folded);

            if (factors.size() == 1)
            {
                if (folded.is_one()) return factor_powers[0];

                // Distribute a number over a sum, so that 2*(x + y) equals 2*x + 2*y
                if (factors[0].base.is_add() && factors[0].exponent == one())
                {
                    Number  constant = 0;
                    TermMap terms;
                    add_term(factors[0].base, folded, constant, terms);
                    return build_add(constant, terms);
                }
            }
            return make_expr(MulData{folded, std::move(factors)});
        }


       /*-------------------------------------------------------------------------------------------
        * Expr make_mul(std::vector<Expr> const& operands)
        *
        * The canonical product of expressions in canonical form.
        *
        * All exponentials combine into one, whose argument is the sum of theirs. That combined
        * exponential can itself simplify, as exp(log(x) + y)*exp(-y) = exp(log(x)) = x, in which
        * case it is multiplied in as an ordinary expression.
        *-----------------------------------------------------------------------------------------*/
        inline Expr make_mul(std::vector<Expr> const& operands)
        {
            Number            coefficient = 1;
            PowerMap          powers;
            std::vector<Expr> exponents;
            for (Expr const& operand : operands)
                multiply_factor(operand, coefficient, powers, exponents);
            if (exponents.empty()) return build_mul(coefficient, powers);

            Expr exponential = make_call(Elementary::Exp, make_add(exponents));
            if (!is_exp(exponential)) return make_mul({build_mul(coefficient, powers), exponential});
            accumulate_power(powers, exponential, one());
            return build_mul(coefficient, powers);
        }


       /*-------------------------------------------------------------------------------------------
        * Expr make_pow(Expr const& base, Expr const& exponent)
        *
        * The canonical power base^exponent.
        *
        * With a numeric exponent n: x^0 = 1 (including 0^0), x^1 = x, and a numeric base is
        * raised exactly when the result is rational. For an integer n, (b^e)^n = b^(e*n), a
        * product is raised factor by factor, and a sum's content is taken out
        * ((2*x + 2)^2 = 4*(x + 1)^2). These rules do not hold for every real base when n is not
        * an integer ((x^2)^(1/2) is |x|, not x), so such powers are left as they are. With any
        * exponent, 1^x = 1 and exp(u)^x = exp(u*x).
        *
        * Throws std::domain_error for zero raised to a negative number.
        *-----------------------------------------------------------------------------------------*/
        inline Expr make_pow(Expr const& base, Expr const& exponent)
        {
            if (is_exp(base))
                return make_call(Elementary::Exp, make_mul({base.node().call().argument, exponent}));

            if (exponent.is_number())
            {
                Number const& n = exponent.number();
                if (n.is_zero()) return one();
                if (n.is_one()) return base;

                if (base.is_number())
                {
                    std::optional<Number> value = exact_pow(base.number(), n);
                    if (value) return Expr(*value);
                    return make_expr(PowData{base, exponent});
                }

                if (n.is_integer() && base.is_pow())
                {
                    PowData const& power = base.node().pow();
                    return make_pow(power.base, make_mul({power.exponent, exponent}));
                }

                // A sum raised to an integer power is kept primitive: (2*x + 2)^2 = 4*(x + 1)^2
                if (n.is_integer() && base.is_add())
                {
                    auto [content, primitive] = split_content(base);
                    if (!content.is_one())
                    {
                        Expr scale(exact_pow(content, n).value());
                        return make_mul({scale, make_pow(primitive, exponent)});
                    }
                }

                if (n.is_integer() && base.is_mul())
                {
                    MulData const& product = base.node().mul();
                    std::vector<Expr> operands{Expr(exact_pow(product.coefficient, n).value())};
                    for (Factor const& factor : product.factors)
                    {
                        Expr power = make_mul({factor.exponent, exponent});
                        operands.push_back(make_pow(factor.base, power));
                    }
                    return make_mul(operands);
                }
            }
            else if (base.is_number() && base.number().is_one())
                return one();

            return make_expr(PowData{base, exponent});
        }


       /*-------------------------------------------------------------------------------------------
        * bool has_leading_minus(Expr const& expr)
        *
        * Whether an expression reads with a leading minus sign: a negative number, a product with
        * a negative coefficient, or a sum whose first term is negative. Exactly one of u and -u
        * has a leading minus (unless u is 0), which lets odd and even functions choose a
        * canonical sign: sin(-x) = -sin(x).
        *-----------------------------------------------------------------------------------------*/
        inline bool has_leading_minus(Expr const& expr)
        {
            switch (expr.kind())
            {
                case Kind::Number:
                    return expr.number().is_negative();
                case Kind::Mul:
                    return expr.node().mul().coefficient.is_negative();
                case Kind::Add:
                    return expr.node().add().terms.front().coefficient.is_negative();
                default:
                    return false;
            }
        }


       /*-------------------------------------------------------------------------------------------
        * std::optional<Expr> separate_logarithms(Expr const& argument)
        *
        * Rewrites exp(argument) with its logarithm terms taken out: exp(c*log(u) + rest) =
        * u^c*exp(rest), for each term c*log(u) of the argument.
        *
        * Returns:
        *     The rewritten exponential, or nothing if the argument has no logarithm terms
        *-----------------------------------------------------------------------------------------*/
        inline std::optional<Expr> separate_logarithms(Expr const& argument)
        {
            // The argument as constant + sum of coefficient*term
            Number            constant = 0;
            std::vector<Term> terms;
            if (argument.is_add())
            {
                constant = argument.node().add().constant;
                terms    = argument.node().add().terms;
            }
            else if (argument.is_mul() && argument.node().mul().factors.size() == 1)
            {
                MulData const& product = argument.node().mul();
                Expr single = make_pow(product.factors[0].base, product.factors[0].exponent);
                terms.push_back(Term{single, product.coefficient});
            }
            else
                terms.push_back(Term{argument, Number(1)});

            std::vector<Expr> factors;
            std::vector<Expr> rest{Expr(constant)};
            for (Term const& term : terms)
            {
                bool logarithm = term.expr.is_call()
                              && term.expr.node().call().function == Elementary::Log;
                if (logarithm)
                {
                    Expr const& inside = term.expr.node().call().argument;
                    factors.push_back(make_pow(inside, Expr(term.coefficient)));
                }
                else
                    rest.push_back(make_mul({Expr(term.coefficient), term.expr}));
            }
            if (factors.empty()) return std::nullopt;

            factors.push_back(make_call(Elementary::Exp, make_add(rest)));
            return make_mul(factors);
        }


       /*-------------------------------------------------------------------------------------------
        * Expr make_call(Elementary function, Expr const& argument)
        *
        * The canonical call of an elementary function. Simplifications that hold for every real
        * argument are applied:
        *
        *     exp(0) = 1, log(1) = 0, sin(0) = 0, cos(0) = 1
        *     exp(c*log(u) + rest) = u^c*exp(rest), so exp(log(u)) = u
        *     log(exp(u)) = u
        *     sin(-u) = -sin(u), cos(-u) = cos(u)
        *
        * Taking logarithms out of an exponential keeps products canonical: otherwise
        * exp(log(x))*exp(y) = x*exp(y) would differ from exp(log(x) + y).
        *
        * A real (floating-point) argument is evaluated, giving a real. Any other exact argument
        * is kept symbolic, so exp(1) stays the exact number e.
        *
        * Throws std::domain_error for log(0).
        *-----------------------------------------------------------------------------------------*/
        inline Expr make_call(Elementary function, Expr const& argument)
        {
            bool   is_number = argument.is_number();
            Number value     = is_number ? argument.number() : Number(0);
            bool   is_real   = is_number && value.is_real();

            switch (function)
            {
                case Elementary::Exp:
                {
                    if (is_number && value.is_zero()) return one();
                    if (is_real) return Expr(std::exp(value.to_double()));
                    std::optional<Expr> separated = separate_logarithms(argument);
                    if (separated) return *separated;
                    break;
                }

                case Elementary::Log:
                    if (is_number && value.is_zero())
                        throw std::domain_error("log: the logarithm of 0 is undefined");
                    if (is_number && value.is_one()) return zero();
                    if (is_real && !value.is_negative()) return Expr(std::log(value.to_double()));
                    if (is_exp(argument)) return argument.node().call().argument;
                    break;

                case Elementary::Sin:
                    if (is_number && value.is_zero()) return zero();
                    if (is_real) return Expr(std::sin(value.to_double()));
                    if (has_leading_minus(argument))
                    {
                        Expr positive = make_mul({Expr(-1), argument});
                        return make_mul({Expr(-1), make_call(function, positive)});
                    }
                    break;

                case Elementary::Cos:
                    if (is_number && value.is_zero()) return one();
                    if (is_real) return Expr(std::cos(value.to_double()));
                    if (has_leading_minus(argument))
                        return make_call(function, make_mul({Expr(-1), argument}));
                    break;
            }
            return make_expr(CallData{function, argument});
        }
    }
}
#endif
