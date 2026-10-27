/*--------------------------------------------------------------------------------------------------
 * File: 04_substitution_and_evaluation.cpp
 *
 * Description: Example 4: substituting into expressions, and evaluating them.
 *
 *              substitute replaces variables by numbers or by other expressions and simplifies
 *              the result. evaluate computes a numeric value; it is the simple, reference way to
 *              evaluate an expression (Function, in example 5, is the fast way).
 *
 *              Build and run from the repository root:
 *
 *                  cmake -S . -B build && cmake --build build
 *                  ./build/examples/04_substitution_and_evaluation
 *------------------------------------------------------------------------------------------------*/
#include <iostream>

#include "lode/lode.hpp"

using namespace lode;

int main()
{
    Symbol x("x");
    Symbol y("y");
    Symbol t("t");

    Expr f = x*y + pow(y, 2) + exp(x - 1);
    std::cout << "f                    = " << f << "\n";

    /*----------------------------------- Substituting numbers -----------------------------------*/

    // Replacements are given as {variable, value} pairs. The result is simplified, so numbers
    // fold together.
    // f with y = 2 is 2*x + exp(x - 1) + 4
    std::cout << "f with y = 2         = " << substitute(f, {{y, 2}}) << "\n";

    // Exact values give exact results
    Expr at_point = substitute(f, {{x, 1}, {y, Number(1, 2)}});
    std::cout << "f at x = 1, y = 1/2  = " << at_point << "\n";                  // 7/4

    /*--------------------------------- Substituting expressions ---------------------------------*/

    // Replacing variables by expressions changes variables (a reparametrization)
    std::cout << "f along x = 2t, y = t = " << substitute(f, {{x, 2*t}, {y, t}}) << "\n";

    // All replacements happen at once, so variables can be swapped
    Expr swapped = substitute(x - 2*y, {{x, y}, {y, x}});
    std::cout << "x - 2y, swapped      = " << swapped << "\n";                   // -2*x + y

    /*---------------------------------- Listing the variables -----------------------------------*/

    std::cout << "variables of f       =";
    for (Symbol const& variable : symbols(f)) std::cout << " " << variable;
    std::cout << "\n";                                                            // x y

    /*------------------------------------ Numeric evaluation ------------------------------------*/

    // evaluate takes a {variable, value} pair for each variable and computes in double
    double value = evaluate(f, {{x, 1.0}, {y, 0.5}});
    std::cout << "f(1.0, 0.5)          = " << value << "\n";                     // 1.75

    // A variable without a value is an error
    try
    {
        evaluate(f, {{x, 1.0}});
    }
    catch (std::invalid_argument const& error)
    {
        std::cout << "error                : " << error.what() << "\n";
    }

    return 0;
}
