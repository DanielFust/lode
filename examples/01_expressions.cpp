/*--------------------------------------------------------------------------------------------------
 * File: 01_expressions.cpp
 *
 * Description: Example 1: symbols, expressions and automatic simplification.
 *
 *              Variables are Symbols; expressions are built from them with ordinary arithmetic.
 *              Every expression is kept in a canonical form, so like terms combine and numbers
 *              fold as you write.
 *
 *              Build and run from the repository root:
 *
 *                  cmake -S . -B build && cmake --build build
 *                  ./build/examples/01_expressions
 *------------------------------------------------------------------------------------------------*/
#include <iostream>

#include "lode/lode.hpp"

using namespace lode;

int main()
{
    std::cout << std::boolalpha;

    /*--------------------------------- Symbols and expressions ----------------------------------*/

    // A Symbol is a variable. Its name is used for printing.
    Symbol x("x");
    Symbol y("y");

    // Expressions are written as in mathematics; integers mix in freely. They print in a fixed
    // order: each variable with its powers (x, then x^2, then y), then products, then constants.
    Expr e = 3*x*x + 2*x*y - y/4 + 1;
    std::cout << "e            = " << e << "\n";                // 3*x^2 - y/4 + 2*x*y + 1

    /*------------------------------- Simplification as you build --------------------------------*/

    // Like terms and like factors combine; terms are kept in a fixed order
    std::cout << "x + y + x    = " << x + y + x << "\n";        // 2*x + y
    std::cout << "x*y*x        = " << x*y*x << "\n";            // x^2*y
    std::cout << "y*x - x*y    = " << y*x - x*y << "\n";        // 0
    std::cout << "x^3/x        = " << pow(x, 3)/x << "\n";      // x^2

    // A sum is a single object, so it can repeat as a factor or cancel as one
    std::cout << "(x+1)*(x+1)  = " << (x + 1)*(x + 1) << "\n";  // (x + 1)^2
    std::cout << "(2x+2)/(x+1) = " << (2*x + 2)/(x + 1) << "\n";// 2

    // Products of sums are not multiplied out: that could make expressions much larger
    std::cout << "x*(y + 1)    = " << x*(y + 1) << "\n";        // x*(y + 1)

    /*-------------------------------------- Exact numbers ---------------------------------------*/

    // Fractions stay exact
    std::cout << "x/3 + x/6    = " << x/3 + x/6 << "\n";        // x/2
    std::cout << "1/3 + 1/6    = " << Expr(1)/3 + Expr(1)/6 << "\n";   // 1/2

    // Floating-point numbers are a separate kind of number, printed with a decimal point. Mixing
    // the two gives a floating-point result.
    std::cout << "0.5*x + x/2  = " << 0.5*x + x/2 << "\n";      // 1.0*x

    /*----------------------------------------- Equality -----------------------------------------*/

    // == asks whether two expressions are the same. Since both are in canonical form, the order
    // and grouping in which they were written do not matter.
    std::cout << "(x+y)^2 == (y+x)*(x+y) : " << (pow(x + y, 2) == (y + x)*(x + y)) << "\n";   // true

    // It compares form, not value: x*(y + 1) and x*y + x are equal as functions but are
    // different expressions, because products are not expanded
    std::cout << "x*(y+1) == x*y + x     : " << (x*(y + 1) == x*y + x) << "\n";            // false

    /*----------------------------- Each Symbol is its own variable ------------------------------*/

    // Two Symbols are different variables even if they share a name; a copy is the same variable
    Symbol another_x("x");
    Symbol same_x = x;
    std::cout << "x == another_x : " << (x == another_x) << "\n";   // false
    std::cout << "x - another_x  = " << x - another_x << "\n";      // x - x (two variables named x)
    std::cout << "x - same_x     = " << x - same_x << "\n";         // 0

    return 0;
}
