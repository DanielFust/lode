/*--------------------------------------------------------------------------------------------------
 * File: substitute_test.cpp
 *
 * Description: Tests of substitution, the basis of partial evaluation, and of listing an
 *              expression's variables.
 *------------------------------------------------------------------------------------------------*/
#include <stdexcept>
#include <vector>

#include "check.hpp"
#include "lode/lode.hpp"

int main()
{
    using lode::Expr;
    using lode::Number;
    using lode::Symbol;

    Symbol x("x");
    Symbol y("y");
    Symbol z("z");
    Symbol t("t");

    // Numbers fold into the result
    LODE_CHECK_EQUAL(substitute(pow(x, 2) + y, {{x, 3}}), y + 9);
    LODE_CHECK_EQUAL(substitute(x*y + pow(y, 2) + z, {{y, 2}}), 2*x + z + 4);
    LODE_CHECK_EQUAL(substitute(x*y, {{y, 0}}), Expr(0));

    // Exact numbers give exact results
    LODE_CHECK_EQUAL(substitute(pow(x, 2) + 1, {{x, Number(1, 2)}}), Expr(Number(5, 4)));
    LODE_CHECK_EQUAL(substitute(x/3 + y/6, {{x, 1}, {y, 1}}), Expr(Number(1, 2)));

    // Functions simplify after substitution
    LODE_CHECK_EQUAL(substitute(exp(x) + log(y), {{x, 0}, {y, 1}}), Expr(1));
    LODE_CHECK_EQUAL(substitute(sin(x)*cos(x), {{x, 0}}), Expr(0));
    LODE_CHECK_THROWS(substitute(log(x), {{x, 0}}), std::domain_error);

    // Replacing by expressions (reparametrization), all at once
    LODE_CHECK_EQUAL(substitute(pow(x, 2), {{x, y + 1}}), pow(y + 1, 2));
    LODE_CHECK_EQUAL(substitute(x - 2*y, {{x, y}, {y, x}}), y - 2*x);
    LODE_CHECK_EQUAL(substitute(x + y, {{x, 2*t}, {y, -2*t}}), Expr(0));
    LODE_CHECK_EQUAL(substitute(exp(x)*exp(y), {{y, -x}}), Expr(1));

    // Parts without the variables are kept as they are, not rebuilt
    Expr f = sin(x)*cos(z) + y;
    LODE_CHECK(&substitute(f, {{t, 1}}).node() == &f.node());

    // Substitution commutes with differentiation
    Expr g = exp(x*y)*sin(x + y);
    LODE_CHECK_EQUAL(substitute(diff(g, x), {{y, 2}}), diff(substitute(g, {{y, 2}}), x));

    LODE_CHECK_THROWS(substitute(x, {{x, 1}, {x, 2}}), std::invalid_argument);

    // Variables, in canonical order, each once
    std::vector<Symbol> found = symbols(z*sin(x) + pow(y, x) + 1);
    LODE_CHECK_EQUAL(found.size(), std::size_t{3});
    if (found.size() == 3)
    {
        LODE_CHECK_EQUAL(found[0], x);
        LODE_CHECK_EQUAL(found[1], y);
        LODE_CHECK_EQUAL(found[2], z);
    }
    LODE_CHECK(symbols(Expr(3)).empty());
    LODE_CHECK_EQUAL(Symbol::from(x + 0).name(), std::string("x"));
    LODE_CHECK_THROWS(Symbol::from(x + 1), std::invalid_argument);

    return lode::test::report();
}
