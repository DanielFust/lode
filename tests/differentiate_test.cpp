/*--------------------------------------------------------------------------------------------------
 * File: differentiate_test.cpp
 *
 * Description: Tests of differentiation: the rules on known derivatives, and random expressions
 *              checked numerically against complex-step derivatives.
 *------------------------------------------------------------------------------------------------*/
#include <algorithm>
#include <cmath>
#include <complex>
#include <stdexcept>

#include "check.hpp"
#include "lode/lode.hpp"
#include "random_expressions.hpp"

namespace
{
    using lode::Expr;
    using lode::Symbol;
    using Complex = std::complex<double>;

   /*-----------------------------------------------------------------------------------------------
    * void check_against_complex_step(lode::test::RandomExpressions& random, int trials,
    *                                 std::vector<Symbol> const& variables)
    *
    * Compares symbolic derivatives of random expressions with complex-step derivatives at
    * random points. The complex step, Im f(x + ih)/h, is free of cancellation error, so the
    * two should agree to near machine precision. Points where the expression or its
    * derivative is not finite (as log of a negative number) are skipped.
    *---------------------------------------------------------------------------------------------*/
    void check_against_complex_step(lode::test::RandomExpressions& random, int trials,
                                    std::vector<Symbol> const& variables)
    {
        double const STEP = 1e-20;
        for (int trial = 0; trial < trials; ++trial)
        {
            Expr f = random.next(3);

            lode::Values<double> point;
            for (Symbol const& variable : variables)
                point.emplace_back(variable, random.uniform_real(0.5, 2.0));

            for (Symbol const& variable : variables)
            {
                double value      = lode::evaluate(f, point);
                double derivative = lode::evaluate(diff(f, variable), point);
                if (!std::isfinite(value) || !std::isfinite(derivative)) continue;

                // The same point, stepped by i*STEP along this variable
                lode::Values<Complex> stepped;
                for (auto const& [symbol, coordinate] : point)
                {
                    double step = symbol == variable ? STEP : 0.0;
                    stepped.emplace_back(symbol, Complex(coordinate, step));
                }
                double reference = lode::evaluate<Complex>(f, stepped).imag() / STEP;

                double tolerance = 1e-9 * std::max(1.0, std::abs(reference));
                LODE_CHECK(std::abs(derivative - reference) <= tolerance);
            }
        }
    }
}


int main()
{
    Symbol x("x");
    Symbol y("y");
    Symbol z("z");

    // Basic rules
    LODE_CHECK_EQUAL(diff(Expr(5), x), Expr(0));
    LODE_CHECK_EQUAL(diff(x, x), Expr(1));
    LODE_CHECK_EQUAL(diff(y, x), Expr(0));
    LODE_CHECK_EQUAL(diff(pow(x, 3), x), 3*pow(x, 2));
    LODE_CHECK_EQUAL(diff(x*y, x), y);
    LODE_CHECK_EQUAL(diff(pow(x, 2)*pow(y, 3), y), 3*pow(x, 2)*pow(y, 2));
    LODE_CHECK_EQUAL(diff(1/x, x), -1/pow(x, 2));
    LODE_CHECK_EQUAL(diff(sqrt(x), x), 1/(2*sqrt(x)));
    LODE_CHECK_EQUAL(diff(pow(x, x), x), pow(x, x)*(log(x) + 1));
    LODE_CHECK_EQUAL(diff(pow(Expr(2), x), x), pow(Expr(2), x)*log(Expr(2)));

    // Chain rule through the elementary functions
    LODE_CHECK_EQUAL(diff(exp(2*x), x), 2*exp(2*x));
    LODE_CHECK_EQUAL(diff(log(pow(x, 2) + 1), x), 2*x/(pow(x, 2) + 1));
    LODE_CHECK_EQUAL(diff(sin(pow(x, 2)), x), 2*x*cos(pow(x, 2)));
    LODE_CHECK_EQUAL(diff(cos(x), x), -sin(x));
    LODE_CHECK_EQUAL(diff(cos(x), x, 2), -cos(x));
    LODE_CHECK_EQUAL(diff(sin(x), x, 4), sin(x));

    // The nablaxx demo: f = 2x^2 + 1 + y + x + xyz
    Expr f = x*x*2 + 1 + y + x + x*y*z;
    LODE_CHECK_EQUAL(diff(f, x), 4*x + y*z + 1);
    LODE_CHECK_EQUAL(diff(f, y), x*z + 1);
    LODE_CHECK_EQUAL(diff(f, z), x*y);

    // Higher and mixed derivatives; mixed partials agree whatever the order
    LODE_CHECK_EQUAL(diff(pow(x, 4), x, 2), 12*pow(x, 2));
    LODE_CHECK_EQUAL(diff(pow(x, 4), x, 0), pow(x, 4));
    Expr g = exp(x*y)*sin(y + z);
    LODE_CHECK_EQUAL(diff(diff(g, x), y), diff(diff(g, y), x));
    LODE_CHECK_EQUAL(diff(diff(g, y), z), diff(diff(g, z), y));
    LODE_CHECK_THROWS(diff(x, x, -1), std::invalid_argument);

    // Arrhenius rate k = A*exp(-Ea/(R*T)): dk/dT = A*Ea/(R*T^2)*exp(-Ea/(R*T))
    Symbol a("A");
    Symbol ea("Ea");
    Symbol r("R");
    Symbol t("T");
    Expr k = a*exp(-ea/(r*t));
    LODE_CHECK_EQUAL(diff(k, t), a*ea/(r*pow(t, 2))*exp(-ea/(r*t)));

    // Random expressions against complex-step derivatives
    lode::test::RandomExpressions random(20261028, {x, y, z}, true);
    check_against_complex_step(random, 1000, {x, y, z});

    return lode::test::report();
}
