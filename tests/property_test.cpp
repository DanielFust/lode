/*--------------------------------------------------------------------------------------------------
 * File: property_test.cpp
 *
 * Description: Randomized tests of the canonical form. Random expressions are combined in
 *              different orders and groupings, and the results must be equal: the algebraic
 *              laws hold structurally, not just in value, as long as arithmetic stays exact.
 *------------------------------------------------------------------------------------------------*/
#include <algorithm>
#include <cmath>
#include <vector>

#include "check.hpp"
#include "lode/lode.hpp"
#include "random_expressions.hpp"

namespace
{
    using lode::Expr;
    using lode::Kind;
    using lode::Symbol;


   /*-----------------------------------------------------------------------------------------------
    * bool has_real(Expr const& expr)
    *
    * Whether an expression contains a floating-point number anywhere, including coefficients.
    * Exact arithmetic that overflows 64 bits falls back to floating point (decision D7), and
    * from then on results depend on the order of rounding.
    *---------------------------------------------------------------------------------------------*/
    bool has_real(Expr const& expr)
    {
        switch (expr.kind())
        {
            case Kind::Number:
                return expr.number().is_real();
            case Kind::Symbol:
                return false;
            case Kind::Add:
            {
                lode::detail::AddData const& sum = expr.node().add();
                if (sum.constant.is_real()) return true;
                for (lode::detail::Term const& term : sum.terms)
                    if (term.coefficient.is_real() || has_real(term.expr)) return true;
                return false;
            }
            case Kind::Mul:
            {
                lode::detail::MulData const& product = expr.node().mul();
                if (product.coefficient.is_real()) return true;
                for (lode::detail::Factor const& factor : product.factors)
                    if (has_real(factor.base) || has_real(factor.exponent)) return true;
                return false;
            }
            case Kind::Pow:
                return has_real(expr.node().pow().base) || has_real(expr.node().pow().exponent);
            case Kind::Call:
                return has_real(expr.node().call().argument);
        }
        return false;
    }


   /*-----------------------------------------------------------------------------------------------
    * bool agree(Expr const& a, Expr const& b, std::vector<Symbol> const& variables)
    *
    * Whether two results of an algebraic law agree. While arithmetic is exact they must be
    * identical, which is the point of the canonical form. Once either contains a floating-point
    * number, they need only be equal in value, to rounding, at a few points.
    *---------------------------------------------------------------------------------------------*/
    bool agree(Expr const& a, Expr const& b, std::vector<Symbol> const& variables)
    {
        if (a == b) return true;
        if (!has_real(a) && !has_real(b)) return false;
        for (double start : {0.7, 1.3, 1.9})
        {
            lode::Values<double> values;
            for (std::size_t i = 0; i < variables.size(); ++i)
                values.emplace_back(variables[i], start + 0.1 * static_cast<double>(i));
            double u = lode::evaluate(a, values);
            double v = lode::evaluate(b, values);
            if (std::isnan(u) || std::isnan(v))
            {
                if (std::isnan(u) != std::isnan(v)) return false;
                continue;
            }
            if (std::abs(u - v) > 1e-9 * std::max({1.0, std::abs(u), std::abs(v)})) return false;
        }
        return true;
    }


   /*-----------------------------------------------------------------------------------------------
    * void check_laws(lode::test::RandomExpressions& random, int trials,
    *                 std::vector<Symbol> const& variables)
    *
    * Checks algebraic laws on triples of random expressions.
    *---------------------------------------------------------------------------------------------*/
    void check_laws(lode::test::RandomExpressions& random, int trials,
                    std::vector<Symbol> const& variables)
    {
        auto same = [&](Expr const& a, Expr const& b) {return agree(a, b, variables);};
        for (int trial = 0; trial < trials; ++trial)
        {
            Expr e = random.next(3);
            Expr f = random.next(3);
            Expr g = random.next(3);

            // Commutative, associative and distributive laws (numbers distribute over sums)
            LODE_CHECK(same(e + f, f + e));
            LODE_CHECK(same(e*f, f*e));
            LODE_CHECK(same((e + f) + g, e + (f + g)));
            LODE_CHECK(same((e*f)*g, e*(f*g)));
            LODE_CHECK(same(3*(e + f), 3*e + 3*f));

            // Inverses
            LODE_CHECK(same(e - e, Expr(0)));
            LODE_CHECK(same((e + f) - f, e));
            if (f != Expr(0))
            {
                LODE_CHECK(same(e*f/f, e));
                LODE_CHECK(same(f/f, Expr(1)));
            }

            // Integer powers
            LODE_CHECK(same(pow(e*f, 2), pow(e, 2)*pow(f, 2)));
            LODE_CHECK(same(pow(e, 2)*e, pow(e, 3)));

            // Exponentials combine
            LODE_CHECK(same(exp(e)*exp(f), exp(e + f)));
            LODE_CHECK(same(exp(e)/exp(e), Expr(1)));

            // Equal expressions hash equally
            if (e + f == f + e) LODE_CHECK_EQUAL((e + f).hash(), (f + e).hash());
        }
    }
}


int main()
{
    lode::Symbol x("x");
    lode::Symbol y("y");
    lode::Symbol z("z");

    // Polynomials and rational functions, then with elementary functions too
    lode::test::RandomExpressions algebraic(20261026, {x, y, z}, false);
    check_laws(algebraic, 2000, {x, y, z});
    lode::test::RandomExpressions elementary(20261027, {x, y, z}, true);
    check_laws(elementary, 2000, {x, y, z});

    return lode::test::report();
}
