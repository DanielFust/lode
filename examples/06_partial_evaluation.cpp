/*--------------------------------------------------------------------------------------------------
 * File: 06_partial_evaluation.cpp
 *
 * Description: Example 6: partial evaluation.
 *
 *              Calling a function with symbols as well as numbers gives a new function. Fixing
 *              some variables this way folds the constants into the new function, which then
 *              evaluates faster than the original.
 *
 *              Build and run from the repository root:
 *
 *                  cmake -S . -B build && cmake --build build
 *                  ./build/examples/06_partial_evaluation
 *------------------------------------------------------------------------------------------------*/
#include <iostream>

#include "lode/lode.hpp"

using namespace lode;

namespace
{
    // Prints a function with its variables and the size of its compiled program
    void show(char const* label, Function const& function)
    {
        std::cout << label << " = " << function << "\n    variables (";
        for (std::size_t i = 0; i < function.domain().size(); ++i)
            std::cout << (i == 0 ? "" : ", ") << function.domain()[i];
        std::cout << "), " << function.instruction_count() << " instructions\n";
    }
}


int main()
{
    Symbol x("x");
    Symbol y("y");
    Symbol z("z");
    Symbol t("t");

    Function f(x, y, z);
    f = x*y + pow(y, 2)*z + exp(y)*x;
    show("f", f);

    /*------------------------------------ Fixing a variable -------------------------------------*/

    // A number fixes a variable; a symbol keeps it. g is a function of (x, z).
    Function g = f(x, 2, z);
    show("g", g);                                        // 2*x + 4*z + x*exp(2)

    // g gives the same values as f with y = 2, with fewer instructions
    std::cout << "g(1.5, 3) = " << g(1.5, 3.0) << ",  f(1.5, 2, 3) = " << f(1.5, 2.0, 3.0) << "\n";

    /*------------------------- Renaming, reordering and reparametrizing -------------------------*/

    // Arguments may be any expressions; the new function's variables are the arguments'
    // variables, in the order they first appear
    show("f(z, y, x)", f(z, y, x));                      // x and z swapped
    show("f(t, t, t)", f(t, t, t));                      // along the diagonal
    show("f(2t, 1, 1 - t)", f(2*t, 1, 1 - t));           // along a line

    /*--------------------------------- Fixing variables by name ---------------------------------*/

    // partial() names the variables to replace and keeps the others in their order
    show("f.partial({{y, 2}})", f.partial({{y, 2}}));

    /*----------------------------- Exact and floating-point values ------------------------------*/

    // An integer argument stays exact, so exp(2) is kept exactly; a floating-point argument
    // becomes a floating-point constant and is evaluated now
    show("f(x, 2, z)  ", f(x, 2, z));
    show("f(x, 2.0, z)", f(x, 2.0, z));

    return 0;
}
