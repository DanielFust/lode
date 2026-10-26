/*--------------------------------------------------------------------------------------------------
 * File: property_test.cpp
 *
 * Description: Randomized tests of the canonical form. Random expressions are combined in
 *              different orders and groupings, and the results must be equal: the algebraic
 *              laws hold structurally, not just in value.
 *------------------------------------------------------------------------------------------------*/
#include "check.hpp"
#include "lode/lode.hpp"
#include "random_expressions.hpp"

namespace
{
    using lode::Expr;

   /*-----------------------------------------------------------------------------------------------
    * void check_laws(lode::test::RandomExpressions& random, int trials)
    *
    * Checks algebraic laws on triples of random expressions.
    *---------------------------------------------------------------------------------------------*/
    void check_laws(lode::test::RandomExpressions& random, int trials)
    {
        for (int trial = 0; trial < trials; ++trial)
        {
            Expr e = random.next(3);
            Expr f = random.next(3);
            Expr g = random.next(3);

            // Commutative, associative and distributive laws (numbers distribute over sums)
            LODE_CHECK_EQUAL(e + f, f + e);
            LODE_CHECK_EQUAL(e*f, f*e);
            LODE_CHECK_EQUAL((e + f) + g, e + (f + g));
            LODE_CHECK_EQUAL((e*f)*g, e*(f*g));
            LODE_CHECK_EQUAL(3*(e + f), 3*e + 3*f);

            // Inverses
            LODE_CHECK_EQUAL(e - e, Expr(0));
            LODE_CHECK_EQUAL((e + f) - f, e);
            if (f != Expr(0))
            {
                LODE_CHECK_EQUAL(e*f/f, e);
                LODE_CHECK_EQUAL(f/f, Expr(1));
            }

            // Integer powers
            LODE_CHECK_EQUAL(pow(e*f, 2), pow(e, 2)*pow(f, 2));
            LODE_CHECK_EQUAL(pow(e, 2)*e, pow(e, 3));

            // Exponentials combine
            LODE_CHECK_EQUAL(exp(e)*exp(f), exp(e + f));
            LODE_CHECK_EQUAL(exp(e)/exp(e), Expr(1));

            // Equal expressions hash equally
            LODE_CHECK_EQUAL((e + f).hash(), (f + e).hash());
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
    check_laws(algebraic, 2000);
    lode::test::RandomExpressions elementary(20261027, {x, y, z}, true);
    check_laws(elementary, 2000);

    return lode::test::report();
}
