/*--------------------------------------------------------------------------------------------------
 * File: canonical_test.cpp
 *
 * Description: Tests that expressions are built in canonical form: like terms and powers
 *              combine, numbers fold, and equal expressions compare equal however they were
 *              written.
 *------------------------------------------------------------------------------------------------*/
#include <stdexcept>

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

    // Like terms combine and numbers fold, whatever the order or grouping
    LODE_CHECK_EQUAL(x + y + x, 2*x + y);
    LODE_CHECK_EQUAL(y*x - x*y, Expr(0));
    LODE_CHECK_EQUAL(x*x*2, 2*pow(x, 2));
    LODE_CHECK_EQUAL(Expr(2)*2*x, 4*x);
    LODE_CHECK_EQUAL((x + y) + z, x + (y + z));
    LODE_CHECK_EQUAL(x*(y*z), (x*y)*z);
    LODE_CHECK_EQUAL(x - x, Expr(0));
    LODE_CHECK_EQUAL((x + y) - (x + y), Expr(0));
    LODE_CHECK_EQUAL(0*x, Expr(0));
    LODE_CHECK_EQUAL(1*x, x);
    LODE_CHECK_EQUAL(x + 0, x);
    LODE_CHECK_EQUAL((x + y).hash(), (y + x).hash());

    // The nablaxx bug: x*y + x must not become x*y + x^2
    LODE_CHECK(x*y + x != x*y + pow(x, 2));
    LODE_CHECK_EQUAL((x*y + x) - x, x*y);

    // Numbers distribute over a sum, so linear combinations cancel
    LODE_CHECK_EQUAL(2*(x + y), 2*x + 2*y);
    LODE_CHECK_EQUAL(2*(x + y) - 2*x, 2*y);
    LODE_CHECK_EQUAL(-(x - y), y - x);

    // Products of sums are not expanded
    LODE_CHECK((x*(y + z)).is_mul());
    LODE_CHECK(x*(y + z) != x*y + x*z);

    // A sum in a product has its content taken out, so grouping does not matter
    LODE_CHECK_EQUAL((2*(x + y))*z, 2*((x + y)*z));
    LODE_CHECK_EQUAL((-x - y)*z, -((x + y)*z));
    LODE_CHECK_EQUAL(2*(x + y)/(x + y), Expr(2));
    LODE_CHECK_EQUAL((x/2 + y/3)*z, (3*x + 2*y)*z/6);
    LODE_CHECK_EQUAL(pow(2*x + 2, 2), 4*pow(x + 1, 2));
    LODE_CHECK_EQUAL(pow(-x - 1, 2), pow(x + 1, 2));
    LODE_CHECK_EQUAL((2*x + 2)/(x + 1), Expr(2));
    LODE_CHECK_EQUAL((2*x + 2)*(x + 1)/2, pow(x + 1, 2));

    // Powers of the same base combine
    LODE_CHECK_EQUAL(x/x, Expr(1));
    LODE_CHECK_EQUAL(pow(x, 3)/x, pow(x, 2));
    LODE_CHECK_EQUAL(x*pow(x, -1), Expr(1));
    LODE_CHECK_EQUAL(pow(x, y)*pow(x, z), pow(x, y + z));
    LODE_CHECK_EQUAL(pow(x + y, 2)*(x + y), pow(x + y, 3));

    // Simple powers
    LODE_CHECK_EQUAL(pow(x, 0), Expr(1));
    LODE_CHECK_EQUAL(pow(x, 1), x);
    LODE_CHECK_EQUAL(pow(Expr(1), x), Expr(1));
    LODE_CHECK_EQUAL(pow(pow(x, 2), 3), pow(x, 6));
    LODE_CHECK_EQUAL(pow(2*x*y, 2), 4*pow(x, 2)*pow(y, 2));
    LODE_CHECK_EQUAL(pow(x/y, -1), y/x);

    // Roots: sqrt(x)^2 = x holds, but sqrt(x^2) = |x| is not simplified to x
    LODE_CHECK_EQUAL(pow(sqrt(x), 2), x);
    LODE_CHECK_EQUAL(sqrt(x)*sqrt(x), x);
    LODE_CHECK(sqrt(pow(x, 2)) != x);
    LODE_CHECK_EQUAL(sqrt(pow(x, 2))*sqrt(pow(x, 2)), pow(x, 2));
    LODE_CHECK_EQUAL(sqrt(2*x)*sqrt(2*x), 2*x);
    LODE_CHECK_EQUAL(sqrt(Expr(4)), Expr(2));
    LODE_CHECK_EQUAL(sqrt(Expr(2))*sqrt(Expr(2)), Expr(2));
    LODE_CHECK(sqrt(Expr(2)).is_pow());

    // Powers of numbers take the integer part out of the exponent, so each has one form
    Expr root2 = sqrt(Expr(2));
    LODE_CHECK_EQUAL(pow(Expr(2), Number(3, 2)), 2*root2);
    LODE_CHECK_EQUAL(pow(Expr(2), Number(-1, 2)), root2/2);
    LODE_CHECK_EQUAL(pow(pow(Expr(2), Number(-1, 2)), 3), pow(Expr(2), Number(-1, 2))/2);
    LODE_CHECK_EQUAL(sqrt(Expr(Number(2, 3))), root2*sqrt(Expr(3))/3);
    LODE_CHECK_EQUAL(pow(Expr(-1), Number(3, 2)), -sqrt(Expr(-1)));
    LODE_CHECK_EQUAL(root2*pow(Expr(2), Number(3, 4)), 2*pow(Expr(2), Number(1, 4)));
    LODE_CHECK_EQUAL(pow(Expr(8), Number(3, 2)), 8*sqrt(Expr(8)));

    // Rational coefficients are exact
    LODE_CHECK_EQUAL(x/2 + x/3, 5*x/6);
    LODE_CHECK_EQUAL(Expr(1)/3 + Expr(1)/6, Expr(Number(1, 2)));

    // Reals stay distinct from exact numbers
    LODE_CHECK(2.0*x != 2*x);
    LODE_CHECK_EQUAL(0.5*x + 0.5*x, 1.0*x);
    LODE_CHECK_EQUAL(x + 0.0, x);

    // Each Symbol is its own variable, even with the same name; copies are the same variable
    Symbol other_x("x");
    Symbol copy_x = x;
    LODE_CHECK(x != other_x);
    LODE_CHECK_EQUAL(copy_x, x);
    LODE_CHECK_EQUAL(x + copy_x, 2*x);
    LODE_CHECK((x + other_x).is_add());

    // Errors
    LODE_CHECK_THROWS(Symbol(""), std::invalid_argument);
    LODE_CHECK_THROWS(x/0, std::domain_error);
    LODE_CHECK_THROWS(pow(Expr(0), -1), std::domain_error);
    LODE_CHECK_THROWS(x.number(), std::logic_error);

    return lode::test::report();
}
