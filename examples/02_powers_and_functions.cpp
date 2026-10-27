/*--------------------------------------------------------------------------------------------------
 * File: 02_powers_and_functions.cpp
 *
 * Description: Example 2: powers, roots and the elementary functions exp, log, sin and cos.
 *
 *              LODE applies a simplification automatically only if it holds for every real
 *              value of the variables. This example shows which ones it applies, and which it
 *              leaves alone.
 *
 *              Build and run from the repository root:
 *
 *                  cmake -S . -B build && cmake --build build
 *                  ./build/examples/02_powers_and_functions
 *------------------------------------------------------------------------------------------------*/
#include <iostream>

#include "lode/lode.hpp"

using namespace lode;

int main()
{
    Symbol x("x");
    Symbol y("y");

    /*------------------------------------- Powers and roots -------------------------------------*/

    // pow(base, exponent); exponents can be integers, fractions or expressions
    std::cout << "(x^2)^3       = " << pow(pow(x, 2), 3) << "\n";   // x^6
    std::cout << "(2*x*y)^2     = " << pow(2*x*y, 2) << "\n";       // 4*x^2*y^2
    std::cout << "x^y * x^2     = " << pow(x, y)*pow(x, 2) << "\n"; // x^(y + 2)
    std::cout << "x^(-2)        = " << pow(x, -2) << "\n";          // 1/x^2

    // sqrt(u) is u^(1/2). Roots of numbers are exact when the result is rational.
    std::cout << "sqrt(x)       = " << sqrt(x) << "\n";             // x^(1/2)
    std::cout << "sqrt(4/9)     = " << sqrt(Expr(4)/9) << "\n";     // 2/3
    std::cout << "sqrt(2)       = " << sqrt(Expr(2)) << "\n";       // 2^(1/2), kept exact
    std::cout << "sqrt(2)^2     = " << sqrt(Expr(2))*sqrt(Expr(2)) << "\n";   // 2

    // sqrt(x)^2 = x holds wherever sqrt(x) is defined, so it is applied. sqrt(x^2) is |x|,
    // not x, so it is left as it is.
    std::cout << "sqrt(x)^2     = " << pow(sqrt(x), 2) << "\n";     // x
    std::cout << "sqrt(x^2)     = " << sqrt(pow(x, 2)) << "\n";     // (x^2)^(1/2)

    /*----------------------------------- Elementary functions -----------------------------------*/

    std::cout << "exp(0)        = " << exp(Expr(0)) << "\n";        // 1
    std::cout << "exp(log(x))   = " << exp(log(x)) << "\n";         // x
    std::cout << "log(exp(x))   = " << log(exp(x)) << "\n";         // x

    // Exponentials combine, in products and powers
    std::cout << "exp(x)*exp(y) = " << exp(x)*exp(y) << "\n";       // exp(x + y)
    std::cout << "exp(x)^2      = " << pow(exp(x), 2) << "\n";      // exp(2*x)
    std::cout << "exp(2*log(x)) = " << exp(2*log(x)) << "\n";       // x^2

    // Sine is odd and cosine even, so a minus sign comes out of the argument
    std::cout << "sin(-x)       = " << sin(-x) << "\n";             // -sin(x)
    std::cout << "cos(y - x)    = " << cos(y - x) << "\n";          // cos(x - y)

    // log(x^2) = 2*log(x) fails for negative x, so it is not applied
    std::cout << "log(x^2)      = " << log(pow(x, 2)) << "\n";      // log(x^2)

    /*---------------------------- Exact and floating-point arguments ----------------------------*/

    // With an exact argument a function stays symbolic: exp(1) is the number e, exactly.
    // With a floating-point argument it is evaluated.
    std::cout << "exp(1)        = " << exp(Expr(1)) << "\n";        // exp(1)
    std::cout << "exp(1.0)      = " << exp(Expr(1.0)) << "\n";      // 2.718281828459045

    return 0;
}
