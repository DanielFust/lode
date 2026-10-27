/*--------------------------------------------------------------------------------------------------
 * File: 07_parameters.cpp
 *
 * Description: Example 7: parameters.
 *
 *              Parameters are inputs that derivatives leave alone. Their values are given at each
 *              evaluation, so they can change without rebuilding the function. Partial
 *              evaluation (example 6) is the other way to give a variable a value: it substitutes
 *              the value when the function is built, so the constant folds in, but a new value
 *              means building a new function. Parameters suit values that change often.
 *
 *              Build and run from the repository root:
 *
 *                  cmake -S . -B build && cmake --build build
 *                  ./build/examples/07_parameters
 *------------------------------------------------------------------------------------------------*/
#include <iostream>
#include <vector>

#include "lode/lode.hpp"

using namespace lode;

int main()
{
    // A Gaussian bump in x, with centre mu and width sigma
    Symbol x("x");
    Symbol mu("mu");
    Symbol sigma("sigma");
    Expr   bump = exp(-pow(x - mu, 2)/(2*pow(sigma, 2)));

    /*-------------------------------- A function with parameters --------------------------------*/

    // The third argument lists the parameters: here x is the variable, mu and sigma parameters
    Function g({x}, bump, {mu, sigma});
    std::cout << "g          = " << g << "\n";

    // Derivatives are with respect to the variable only. (grad() would give the same derivative
    // as a vector with one entry.)
    Function slope = g.diff(x);
    std::cout << "dg/dx      = " << slope << "\n";

    /*----------------------------- Evaluating with parameter values -----------------------------*/

    // A function's inputs are its variables followed by its parameters: g(x, mu, sigma)
    std::cout << "g(1, 0, 1) = " << g(1.0, 0.0, 1.0) << "\n";             // 0.606531

    // The parameter values can also be passed separately, and changed freely
    std::vector<double> point{1.0};
    std::vector<double> value(1);
    for (double centre : {0.0, 0.5, 1.0})
    {
        std::vector<double> parameters{centre, 1.0};
        g.evaluate(point, parameters, value);
        std::cout << "g(1) with mu = " << centre << ": " << value[0] << "\n";
    }

    /*------------------------- Mixing parameters and partial evaluation -------------------------*/

    // Fix sigma for good, keep mu a parameter
    Function narrow = g.partial({{sigma, Number(1, 10)}});
    std::cout << "with sigma = 1/10: " << narrow << ", parameters:";
    for (Symbol const& parameter : narrow.parameters()) std::cout << " " << parameter;
    std::cout << "\n";

    /*-------------------------------------- Sensitivities ---------------------------------------*/

    // diff with respect to a parameter gives how the output changes with it
    std::cout << "dg/dmu     = " << g.diff(mu) << "\n";

    return 0;
}
