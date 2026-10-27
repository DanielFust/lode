/*--------------------------------------------------------------------------------------------------
 * File: 05_functions.cpp
 *
 * Description: Example 5: functions.
 *
 *              A Function binds expressions to an ordered list of variables (its domain) and
 *              compiles them for fast evaluation. Its outputs can be a scalar, a vector or a
 *              matrix; its gradient adds one dimension.
 *
 *              Build and run from the repository root:
 *
 *                  cmake -S . -B build && cmake --build build
 *                  ./build/examples/05_functions
 *------------------------------------------------------------------------------------------------*/
#include <iostream>
#include <vector>

#include "lode/lode.hpp"

using namespace lode;

int main()
{
    Symbol x("x");
    Symbol y("y");
    Symbol z("z");

    /*------------------------------------ A scalar function -------------------------------------*/

    // Declare the function's variables, then define it
    Function f(x, y, z);
    f = x*x*2 + 1 + y + x + x*y*z;
    std::cout << "f(x, y, z) = " << f << "\n";               // x + 2*x^2 + y + x*y*z + 1

    // Call it with one number per variable, in order
    std::cout << "f(1, 2, 3) = " << f(1.0, 2.0, 3.0) << "\n";   // 12

    /*--------------------------------------- Derivatives ----------------------------------------*/

    // grad() differentiates with respect to every variable: a scalar gives a vector
    Function gradient = f.grad();
    std::cout << "grad f     = " << gradient << "\n";        // [4*x + y*z + 1, x*z + 1, x*y]

    // hessian() is grad().grad(): a 3x3 matrix
    Function hessian = f.hessian();
    std::cout << "hessian f  = " << hessian << "\n";         // [[4, z, y], [z, 0, x], [y, x, 0]]

    // A function's outputs are a tensor of expressions; index it with (), counting from 1
    std::cout << "df/dy      = " << gradient.outputs()(2) << "\n";        // x*z + 1
    std::cout << "d2f/dxdz   = " << hessian.outputs()(1, 3) << "\n";      // y

    /*-------------------------- Evaluating vector and matrix functions --------------------------*/

    // evaluate returns the values as a tensor of numbers, with the outputs' shape. The inputs
    // can be a std::vector or any contiguous storage (they are passed as a std::span).
    std::vector<double> point{1.0, 2.0, 3.0};
    Tensor<double> values = hessian.evaluate(point);
    std::cout << "hessian f at (1, 2, 3) = " << values << "\n";   // [[4, 3, 2], [3, 0, 1], [2, 1, 0]]
    std::cout << "its (1, 2) entry        = " << values(1, 2) << "\n";   // 3

    // To avoid allocating, pass storage for the outputs; they are written row by row
    std::vector<double> h(9);
    hessian.evaluate(point, h);
    std::cout << "hessian f, row by row   =";
    for (double value : h) std::cout << " " << value;       // 4 3 2 3 0 1 2 1 0
    std::cout << "\n";

    /*------------------------------ Vector functions and Jacobians ------------------------------*/

    // A vector function has one output per expression; its gradient is its Jacobian matrix
    Function polar({x, y}, {x*cos(y), x*sin(y)});
    Function jacobian = polar.grad();
    // J = [[cos(y), -x*sin(y)], [sin(y), x*cos(y)]]
    std::cout << "J          = " << jacobian << "\n";
    std::cout << "J's shape  = " << jacobian.shape()[0] << " x " << jacobian.shape()[1] << "\n";

    /*----------------------------------- What evaluation runs -----------------------------------*/

    // Functions are compiled into a short list of instructions, with shared work done once.
    // listing() shows it: here x*z is computed once and used twice.
    Function shared({x, y, z}, std::vector<Expr>{x*z + y, x*z - y});
    std::cout << "instructions for [x*z + y, x*z - y]:\n" << shared.listing();

    return 0;
}
