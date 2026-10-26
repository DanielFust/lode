/*--------------------------------------------------------------------------------------------------
 * File: functions_test.cpp
 *
 * Description: Tests of the elementary functions exp, log, sin and cos: their simplifications,
 *              their place in sums and products, and their text form.
 *------------------------------------------------------------------------------------------------*/
#include <cmath>
#include <stdexcept>
#include <string>

#include "check.hpp"
#include "lode/lode.hpp"

int main()
{
    using lode::Expr;
    using lode::Symbol;
    using Text = std::string;

    Symbol x("x");
    Symbol y("y");

    // Values at special points
    LODE_CHECK_EQUAL(exp(Expr(0)), Expr(1));
    LODE_CHECK_EQUAL(log(Expr(1)), Expr(0));
    LODE_CHECK_EQUAL(sin(Expr(0)), Expr(0));
    LODE_CHECK_EQUAL(cos(Expr(0)), Expr(1));
    LODE_CHECK_THROWS(log(Expr(0)), std::domain_error);

    // Exact arguments stay symbolic; real arguments are evaluated
    LODE_CHECK(exp(Expr(1)).is_call());
    LODE_CHECK(log(Expr(2)).is_call());
    LODE_CHECK_EQUAL(exp(Expr(2.0)), Expr(std::exp(2.0)));
    LODE_CHECK_EQUAL(sin(Expr(0.5)), Expr(std::sin(0.5)));
    LODE_CHECK(log(Expr(-2.0)).is_call());

    // Inverses
    LODE_CHECK_EQUAL(exp(log(x)), x);
    LODE_CHECK_EQUAL(log(exp(x)), x);
    LODE_CHECK_EQUAL(log(exp(x + y)), x + y);

    // Exponentials combine in products and powers
    LODE_CHECK_EQUAL(exp(x)*exp(y), exp(x + y));
    LODE_CHECK_EQUAL(exp(x)*exp(-x), Expr(1));
    LODE_CHECK_EQUAL(exp(x)/exp(y), exp(x - y));
    LODE_CHECK_EQUAL(pow(exp(x), 2), exp(2*x));
    LODE_CHECK_EQUAL(pow(exp(x), y), exp(x*y));
    LODE_CHECK_EQUAL((2*exp(x))*(3*exp(y)), 6*exp(x + y));
    LODE_CHECK_EQUAL(exp(log(x) + y)*exp(-y), x);

    // Odd and even functions take a canonical sign
    LODE_CHECK_EQUAL(sin(-x), -sin(x));
    LODE_CHECK_EQUAL(cos(-x), cos(x));
    LODE_CHECK_EQUAL(sin(y - x), -sin(x - y));
    LODE_CHECK_EQUAL(cos(y - x), cos(x - y));
    LODE_CHECK_EQUAL(sin(-2*x) + sin(2*x), Expr(0));

    // Calls are terms and factors like any other expression
    LODE_CHECK_EQUAL(sin(x) + sin(x), 2*sin(x));
    LODE_CHECK_EQUAL(sin(x)*sin(x), pow(sin(x), 2));
    LODE_CHECK_EQUAL(sin(x)/sin(x), Expr(1));
    LODE_CHECK(sin(x) != cos(x));
    LODE_CHECK(sin(x) != sin(y));

    // Text form
    LODE_CHECK_EQUAL(exp(x).to_string(), Text("exp(x)"));
    LODE_CHECK_EQUAL(sin(x + y).to_string(), Text("sin(x + y)"));
    LODE_CHECK_EQUAL((2*exp(-x)).to_string(), Text("2*exp(-x)"));
    LODE_CHECK_EQUAL(pow(cos(x), 2).to_string(), Text("cos(x)^2"));
    LODE_CHECK_EQUAL((x + sin(x) + x*y).to_string(), Text("x + sin(x) + x*y"));
    LODE_CHECK_EQUAL((1/log(x)).to_string(), Text("1/log(x)"));

    return lode::test::report();
}
