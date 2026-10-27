/*--------------------------------------------------------------------------------------------------
 * File: 03_differentiation.cpp
 *
 * Description: Example 3: derivatives.
 *
 *              diff(expression, variable) differentiates exactly, by the sum, product and chain
 *              rules, and returns an expression in canonical form.
 *
 *              Build and run from the repository root:
 *
 *                  cmake -S . -B build && cmake --build build
 *                  ./build/examples/03_differentiation
 *------------------------------------------------------------------------------------------------*/
#include <iostream>
#include <vector>

#include "lode/lode.hpp"

using namespace lode;

int main()
{
    std::cout << std::boolalpha;

    Symbol x("x");
    Symbol y("y");

    /*---------------------------------- Single-variable rules -----------------------------------*/

    std::cout << "d/dx x^3         = " << diff(pow(x, 3), x) << "\n";       // 3*x^2
    std::cout << "d/dx 1/x         = " << diff(1/x, x) << "\n";             // -1/x^2
    std::cout << "d/dx sqrt(x)     = " << diff(sqrt(x), x) << "\n";         // 1/(2*x^(1/2))
    std::cout << "d/dx sin(x^2)    = " << diff(sin(pow(x, 2)), x) << "\n";  // 2*x*cos(x^2)
    std::cout << "d/dx log(x^2+1)  = " << diff(log(pow(x, 2) + 1), x) << "\n";   // 2*x/(x^2 + 1)
    std::cout << "d/dx x*exp(-x)   = " << diff(x*exp(-x), x) << "\n";       // exp(-x) - x*exp(-x)
    std::cout << "d/dx x^x         = " << diff(pow(x, x), x) << "\n";       // x^x*(log(x) + 1)

    /*------------------------------------ Higher derivatives ------------------------------------*/

    // A third argument gives the order
    std::cout << "d2/dx2 x^4       = " << diff(pow(x, 4), x, 2) << "\n";    // 12*x^2
    std::cout << "d4/dx4 sin(x)    = " << diff(sin(x), x, 4) << "\n";       // sin(x)

    /*----------------------------------- Partial derivatives ------------------------------------*/

    // Variables other than the one differentiated with respect to are constants
    Expr f = pow(x, 2)*y + sin(x*y);
    std::cout << "f                = " << f << "\n";
    std::cout << "df/dx            = " << diff(f, x) << "\n";               // 2*x*y + y*cos(x*y)
    std::cout << "df/dy            = " << diff(f, y) << "\n";               // x^2 + x*cos(x*y)

    // Mixed partial derivatives come out the same in either order
    Expr fxy = diff(diff(f, x), y);
    Expr fyx = diff(diff(f, y), x);
    std::cout << "d2f/dxdy         = " << fxy << "\n";
    std::cout << "same either way  : " << (fxy == fyx) << "\n";             // true

    // A gradient is the list of partial derivatives (Function, in example 5, builds it for you)
    std::vector<Symbol> variables{x, y};
    std::cout << "grad f           = [";
    for (std::size_t i = 0; i < variables.size(); ++i)
        std::cout << (i == 0 ? "" : ", ") << diff(f, variables[i]);
    std::cout << "]\n";

    return 0;
}
