/*--------------------------------------------------------------------------------------------------
 * File: print_test.cpp
 *
 * Description: Tests of the text form of expressions.
 *------------------------------------------------------------------------------------------------*/
#include <sstream>
#include <string>

#include "check.hpp"
#include "lode/lode.hpp"

int main()
{
    using lode::Expr;
    using lode::Number;
    using lode::Symbol;
    using Text = std::string;

    Symbol x("x");
    Symbol y("y");
    Symbol z("z");

    // The nablaxx demo function, in canonical order: x before x^2 before y, products last
    LODE_CHECK_EQUAL((x*x*2 + 1 + y + x + x*y*z).to_string(), Text("x + 2*x^2 + y + x*y*z + 1"));
    LODE_CHECK_EQUAL((x*y + x).to_string(), Text("x + x*y"));

    // Signs
    LODE_CHECK_EQUAL((x - y).to_string(), Text("x - y"));
    LODE_CHECK_EQUAL((y - x).to_string(), Text("-x + y"));
    LODE_CHECK_EQUAL((-x).to_string(), Text("-x"));
    LODE_CHECK_EQUAL((x - 1).to_string(), Text("x - 1"));
    LODE_CHECK_EQUAL((-2*x*y).to_string(), Text("-2*x*y"));

    // Quotients
    LODE_CHECK_EQUAL((x/y).to_string(), Text("x/y"));
    LODE_CHECK_EQUAL((1/x).to_string(), Text("1/x"));
    LODE_CHECK_EQUAL((x/(y*z)).to_string(), Text("x/(y*z)"));
    LODE_CHECK_EQUAL((x/2).to_string(), Text("x/2"));
    LODE_CHECK_EQUAL((-3*x/(4*y)).to_string(), Text("-3*x/(4*y)"));
    LODE_CHECK_EQUAL(pow(x, -2).to_string(), Text("1/x^2"));

    // Powers and parentheses
    LODE_CHECK_EQUAL((x*(y + z)).to_string(), Text("x*(y + z)"));
    LODE_CHECK_EQUAL(pow(x + y, 2).to_string(), Text("(x + y)^2"));
    LODE_CHECK_EQUAL(sqrt(x).to_string(), Text("x^(1/2)"));
    LODE_CHECK_EQUAL(pow(x, y).to_string(), Text("x^y"));
    LODE_CHECK_EQUAL(pow(x, -y).to_string(), Text("x^(-y)"));
    LODE_CHECK_EQUAL(pow(x, y + 1).to_string(), Text("x^(y + 1)"));
    LODE_CHECK_EQUAL(sqrt(Expr(2)).to_string(), Text("2^(1/2)"));
    LODE_CHECK_EQUAL(sqrt(pow(x, 2)).to_string(), Text("(x^2)^(1/2)"));
    LODE_CHECK_EQUAL(sqrt(2*x).to_string(), Text("(2*x)^(1/2)"));

    // Numbers
    LODE_CHECK_EQUAL(Expr(Number(-1, 2)).to_string(), Text("-1/2"));
    LODE_CHECK_EQUAL((2.5*x - 1).to_string(), Text("2.5*x - 1"));
    LODE_CHECK_EQUAL((1.0*x).to_string(), Text("1.0*x"));
    LODE_CHECK_EQUAL((x + Number(1, 2)).to_string(), Text("x + 1/2"));

    // Stream output
    std::ostringstream out;
    out << x + 1;
    LODE_CHECK_EQUAL(out.str(), Text("x + 1"));

    return lode::test::report();
}
