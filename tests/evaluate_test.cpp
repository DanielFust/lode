/*--------------------------------------------------------------------------------------------------
 * File: evaluate_test.cpp
 *
 * Description: Tests of the reference evaluator.
 *------------------------------------------------------------------------------------------------*/
#include <algorithm>
#include <cmath>
#include <complex>
#include <stdexcept>

#include "check.hpp"
#include "lode/lode.hpp"

namespace
{
    // Whether two doubles agree to a relative tolerance
    bool close(double a, double b) {return std::abs(a - b) <= 1e-14 * std::max(1.0, std::abs(b));}
}


int main()
{
    using lode::Expr;
    using lode::Number;
    using lode::Symbol;

    Symbol x("x");
    Symbol y("y");
    Symbol z("z");

    // Polynomials and quotients
    LODE_CHECK_EQUAL(lode::evaluate(pow(x, 2) + 3*y, {{x, 2.0}, {y, 1.0}}), 7.0);
    LODE_CHECK_EQUAL(lode::evaluate(x/y, {{x, 1.0}, {y, 4.0}}), 0.25);
    LODE_CHECK_EQUAL(lode::evaluate(Expr(Number(1, 4)), {}), 0.25);
    LODE_CHECK_EQUAL(lode::evaluate(pow(x, -3), {{x, -2.0}}), -0.125);

    // The nablaxx demo at (1, 2, 3): 1 + 2 + 2 + 1 + 6 = 12
    Expr f = x*x*2 + 1 + y + x + x*y*z;
    LODE_CHECK_EQUAL(lode::evaluate(f, {{x, 1.0}, {y, 2.0}, {z, 3.0}}), 12.0);

    // Elementary functions and non-integer powers
    double exp_log = lode::evaluate(exp(x) + log(y), {{x, 0.5}, {y, 3.0}});
    double sin_cos = lode::evaluate(sin(x)*cos(y), {{x, 0.3}, {y, 0.7}});
    LODE_CHECK(close(exp_log, std::exp(0.5) + std::log(3.0)));
    LODE_CHECK(close(sin_cos, std::sin(0.3) * std::cos(0.7)));
    LODE_CHECK(close(lode::evaluate(sqrt(x), {{x, 2.0}}), std::sqrt(2.0)));
    LODE_CHECK(close(lode::evaluate(pow(x, y), {{x, 2.0}, {y, 0.5}}), std::sqrt(2.0)));
    LODE_CHECK(std::isnan(lode::evaluate(log(x), {{x, -1.0}})));

    // Agrees with exact substitution
    Expr g = pow(x, 3)/7 - x*y/3 + Number(5, 2);
    Expr exact = substitute(g, {{x, Number(3, 2)}, {y, -4}});
    LODE_CHECK(close(lode::evaluate(g, {{x, 1.5}, {y, -4.0}}), exact.number().to_double()));

    // Other number types
    std::complex<double> value = lode::evaluate<std::complex<double>>(pow(x, 2), {{x, {0.0, 1.0}}});
    LODE_CHECK(close(value.real(), -1.0) && close(value.imag(), 0.0));
    LODE_CHECK_EQUAL(lode::evaluate<float>(x + 1, {{x, 1.5f}}), 2.5f);

    // Missing or repeated values
    LODE_CHECK_THROWS(lode::evaluate(x + y, {{x, 1.0}}), std::invalid_argument);
    LODE_CHECK_THROWS(lode::evaluate(x, {{x, 1.0}, {x, 2.0}}), std::invalid_argument);
    LODE_CHECK_EQUAL(lode::evaluate(x, {{x, 1.0}, {y, 2.0}}), 1.0);

    return lode::test::report();
}
