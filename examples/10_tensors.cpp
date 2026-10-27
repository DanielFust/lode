/*--------------------------------------------------------------------------------------------------
 * File: 10_tensors.cpp
 *
 * Description: Example 10: tensors.
 *
 *              A Tensor is an array of any rank. Functions use tensors for their variables, their
 *              outputs and their values. Entries are indexed with (), counting from 1, as in the
 *              mathematics; the storage underneath is indexed with [], counting from 0.
 *
 *              Build and run from the repository root:
 *
 *                  cmake -S . -B build && cmake --build build
 *                  ./build/examples/10_tensors
 *------------------------------------------------------------------------------------------------*/
#include <iostream>
#include <vector>

#include "lode/lode.hpp"

using namespace lode;

int main()
{
    /*------------------------------ Building and indexing tensors -------------------------------*/

    // Braces list the elements: a vector, or a matrix row by row
    Tensor<double> a{{2.0, 1.0},
                     {1.0, 3.0}};
    std::cout << "a       = " << a << "\n";                       // [[2, 1], [1, 3]]

    // Parentheses give the dimensions; the elements start at zero
    Tensor<double> zeros(2, 3);
    std::cout << "zeros   = " << zeros << "\n";                   // [[0, 0, 0], [0, 0, 0]]

    // (i, j) is the entry in row i, column j, counting from 1. [k] is the k-th element of the
    // storage, counting from 0; the storage is row by row.
    std::cout << "a(1, 2) = " << a(1, 2) << ", a[1] = " << a[1] << "\n";   // 1, 1
    std::cout << "a(2, 1) = " << a(2, 1) << ", a[2] = " << a[2] << "\n";   // 1, 1

    /*------------------------------ Variables arranged as a tensor ------------------------------*/

    // make_symbols creates a tensor of new variables, named by their indices
    Tensor<Symbol> x = make_symbols("x", {2});
    Tensor<Symbol> F = make_symbols("F", {2, 2});
    std::cout << "x       = " << x << "\n";                       // [x1, x2]
    std::cout << "F       = " << F << "\n";                       // [[F11, F12], [F21, F22]]

    /*---------------------------------- A function of a vector ----------------------------------*/

    // The quadratic form q(x) = x^T A x, written with indices as in the mathematics. A is a
    // tensor of expressions here, so its entries stay exact (with the numbers in `a`, they
    // would be floating-point: 2.0*x1^2).
    Tensor<Expr> A{{2, 1},
                   {1, 3}};
    Expr q = 0;
    for (std::size_t i = 1; i <= 2; ++i)
        for (std::size_t j = 1; j <= 2; ++j) q += x(i)*A(i, j)*x(j);
    Function form(x, q);
    std::cout << "q(x)    = " << form << "\n";                    // 2*x1^2 + 3*x2^2 + 2*x1*x2

    // Its gradient is 2*A*x and its Hessian 2*A
    std::cout << "grad q  = " << form.grad() << "\n";             // [4*x1 + 2*x2, 2*x1 + 6*x2]
    std::cout << "hess q  = " << form.hessian() << "\n";          // [[4, 2], [2, 6]]

    /*---------------------------------- A function of a matrix ----------------------------------*/

    // The gradient appends the variable's shape, so a scalar function of a 2x2 matrix has a
    // 2x2 gradient: the gradient of the determinant is the cofactor matrix
    Function det(F, F(1, 1)*F(2, 2) - F(1, 2)*F(2, 1));
    Function cofactor = det.grad();
    std::cout << "det F   = " << det << "\n";
    std::cout << "d det/dF = " << cofactor << "\n";               // [[F22, -F21], [-F12, F11]]

    // ... and its second derivative is a tensor of rank 4. (Keep the function in a variable
    // before looping over its shape: det.hessian().shape() would refer into a temporary that is
    // gone by the time the loop runs.)
    Function second = det.hessian();
    std::cout << "d2 det/dF2 has shape";
    for (std::size_t dimension : second.shape()) std::cout << " " << dimension;
    std::cout << "\n";                                            // 2 2 2 2

    /*---------------------------- Expressions and values are tensors ----------------------------*/

    // A function's outputs are a tensor of expressions
    std::cout << "entry (1, 2) of d det/dF: " << cofactor.outputs()(1, 2) << "\n";   // -F21

    // Evaluating gives a tensor of numbers with the outputs' shape. The inputs are the
    // variables in storage order: F11, F12, F21, F22.
    Tensor<double> values = cofactor.evaluate(std::vector<double>{1.0, 2.0, 3.0, 4.0});
    std::cout << "d det/dF at [[1, 2], [3, 4]] = " << values << "\n";   // [[4, -3], [-2, 1]]
    std::cout << "its entry (2, 1)            = " << values(2, 1) << "\n";   // -2

    // A scalar function is still called with numbers and returns a number
    std::cout << "det of [[1, 2], [3, 4]]     = " << det(1.0, 2.0, 3.0, 4.0) << "\n";   // -2

    return 0;
}
