/*--------------------------------------------------------------------------------------------------
 * File: tensor_test.cpp
 *
 * Description: Tests of Tensor (construction, indexing from 1 with () and from 0 with [],
 *              errors, printing), make_symbols, and functions whose domains and outputs are
 *              tensors.
 *------------------------------------------------------------------------------------------------*/
#include <cstddef>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "check.hpp"
#include "lode/lode.hpp"

namespace
{
    // The text form of anything printable
    template<class T>
    std::string text(T const& value)
    {
        std::ostringstream out;
        out << value;
        return out.str();
    }
}


int main()
{
    using lode::Expr;
    using lode::Function;
    using lode::Number;
    using lode::Shape;
    using lode::Symbol;
    using lode::Tensor;
    using Text = std::string;

    /*--------------------------------------- Construction ---------------------------------------*/

    // Parentheses give dimensions, braces give elements, as with std::vector
    Tensor<double> zeros(2, 3);
    LODE_CHECK_EQUAL(zeros.rank(), std::size_t{2});
    LODE_CHECK_EQUAL(zeros.size(), std::size_t{6});
    LODE_CHECK_EQUAL(zeros(2, 3), 0.0);
    Tensor<double> listed{2, 3};
    LODE_CHECK_EQUAL(listed.rank(), std::size_t{1});
    LODE_CHECK_EQUAL(listed(2), 3.0);

    Tensor<int> matrix{{1, 2, 3}, {4, 5, 6}};
    LODE_CHECK(matrix.shape() == (std::vector<std::size_t>{2, 3}));
    Tensor<int> cube(Shape{2, 2, 2}, {1, 2, 3, 4, 5, 6, 7, 8});
    LODE_CHECK_EQUAL(cube(2, 1, 2), 6);
    Tensor<double> scalar;
    LODE_CHECK_EQUAL(scalar.rank(), std::size_t{0});
    LODE_CHECK_EQUAL(scalar.size(), std::size_t{1});
    Tensor<int> from_vector = std::vector<int>{7, 8};
    LODE_CHECK_EQUAL(from_vector.rank(), std::size_t{1});

    /*----------------------------------------- Indexing -----------------------------------------*/

    // () counts from 1, one index per dimension; [] is the storage offset, counted from 0, in
    // row-major order
    LODE_CHECK_EQUAL(matrix(1, 1), 1);
    LODE_CHECK_EQUAL(matrix(1, 3), 3);
    LODE_CHECK_EQUAL(matrix(2, 1), 4);
    LODE_CHECK_EQUAL(matrix[0], 1);
    LODE_CHECK_EQUAL(matrix[3], 4);
    matrix(2, 2) = 50;
    LODE_CHECK_EQUAL(matrix[4], 50);
    scalar() = 2.5;
    LODE_CHECK_EQUAL(scalar[0], 2.5);

    // Errors name the problem
    LODE_CHECK_THROWS(matrix(0, 1), std::out_of_range);
    LODE_CHECK_THROWS(matrix(3, 1), std::out_of_range);
    LODE_CHECK_THROWS(matrix(1, 4), std::out_of_range);
    LODE_CHECK_THROWS(matrix(1), std::out_of_range);
    LODE_CHECK_THROWS(matrix(1, 1, 1), std::out_of_range);
    LODE_CHECK_THROWS((Tensor<int>{{1, 2}, {3}}), std::invalid_argument);
    LODE_CHECK_THROWS(Tensor<int>(Shape{2, 2}, {1, 2, 3}), std::invalid_argument);
    try
    {
        static_cast<void>(matrix(0, 1));
    }
    catch (std::out_of_range const& error)
    {
        Text expected = "Tensor: index 0 of dimension 1 is outside 1 to 2 (indices count from 1)";
        LODE_CHECK_EQUAL(Text(error.what()), expected);
    }

    // Comparison and printing
    LODE_CHECK((Tensor<int>{1, 2} == Tensor<int>{1, 2}));
    LODE_CHECK(!(Tensor<int>{1, 2} == Tensor<int>{{1, 2}}));
    LODE_CHECK_EQUAL(text(Tensor<int>{{1, 2}, {3, 4}}), Text("[[1, 2], [3, 4]]"));
    LODE_CHECK_EQUAL(text(cube), Text("[[[1, 2], [3, 4]], [[5, 6], [7, 8]]]"));
    LODE_CHECK_EQUAL(text(scalar), Text("2.5"));

    /*----------------------------------- Tensors of variables -----------------------------------*/

    Tensor<Symbol> f = lode::make_symbols("F", {2, 2});
    LODE_CHECK_EQUAL(text(f), Text("[[F11, F12], [F21, F22]]"));
    LODE_CHECK(f(1, 2) != f(2, 1));
    Tensor<Symbol> long_vector = lode::make_symbols("v", {12});
    LODE_CHECK_EQUAL(long_vector(10).name(), Text("v_10"));

    /*------------------------------ Functions of a vector variable ------------------------------*/

    // The Jacobian's derivative index comes last: J(i, j) = dF_i/dx_j
    Symbol x("x");
    Symbol y("y");
    Function pair({x, y}, {x*y, x + 2*y});
    Function jacobian = pair.grad();
    LODE_CHECK_EQUAL(jacobian.outputs()(1, 2), x);
    LODE_CHECK_EQUAL(jacobian.outputs()(2, 1), Expr(1));
    LODE_CHECK_EQUAL(jacobian.outputs()(2, 2), Expr(2));

    // A quadratic form x^T A x has gradient (A + A^T) x and Hessian A + A^T
    Tensor<Symbol> v = lode::make_symbols("x", {2});
    Tensor<Expr>   a{{2, 1}, {1, 3}};
    Expr quadratic = 0;
    for (std::size_t i = 1; i <= 2; ++i)
        for (std::size_t j = 1; j <= 2; ++j) quadratic += v(i)*a(i, j)*v(j);
    Function form(v, quadratic);
    LODE_CHECK_EQUAL(form.grad().outputs()(1), 4*v(1) + 2*v(2));
    LODE_CHECK((form.hessian().outputs() == Tensor<Expr>{{4, 2}, {2, 6}}));

    /*------------------------------ Functions of a matrix variable ------------------------------*/

    // The gradient of a determinant is its cofactor matrix
    Expr determinant = f(1, 1)*f(2, 2) - f(1, 2)*f(2, 1);
    Function det(f, determinant);
    LODE_CHECK_EQUAL(det(1.0, 2.0, 3.0, 4.0), -2.0);
    Function cofactor = det.grad();
    LODE_CHECK(cofactor.shape() == (std::vector<std::size_t>{2, 2}));
    LODE_CHECK((cofactor.outputs() == Tensor<Expr>{{f(2, 2), -f(2, 1)}, {-f(1, 2), f(1, 1)}}));

    // Its Hessian has rank 4: d2(det)/dF11 dF22 = 1 and d2(det)/dF12 dF21 = -1
    Function second = det.hessian();
    LODE_CHECK(second.shape() == (std::vector<std::size_t>{2, 2, 2, 2}));
    LODE_CHECK_EQUAL(second.outputs()(1, 1, 2, 2), Expr(1));
    LODE_CHECK_EQUAL(second.outputs()(1, 2, 2, 1), Expr(-1));
    LODE_CHECK_EQUAL(second.outputs()(1, 1, 1, 1), Expr(0));

    // Evaluation returns a tensor with the outputs' shape
    Tensor<double> at_point = cofactor.evaluate(std::vector<double>{1.0, 2.0, 3.0, 4.0});
    LODE_CHECK((at_point == Tensor<double>{{4.0, -3.0}, {-2.0, 1.0}}));
    LODE_CHECK_EQUAL(at_point(1, 2), -3.0);

    // Half the sum of squares has the matrix itself as its gradient
    Expr half_squares = 0;
    for (Symbol const& entry : f) half_squares += pow(entry, 2)/2;
    Function squares(f, half_squares);
    Tensor<Expr> gradient = squares.grad().outputs();
    LODE_CHECK_EQUAL(gradient(2, 1), f(2, 1));

    /*---------------------------- Domains through partial evaluation ----------------------------*/

    // Replacing a parameter keeps the domain's shape; replacing a variable of the domain makes
    // the domain a list
    Symbol scale("s");
    Function scaled(f, scale*determinant, {scale});
    LODE_CHECK(scaled.partial({{scale, 2}}).grad().shape() == (std::vector<std::size_t>{2, 2}));
    Function fixed = det.partial({{f(1, 1), 1}});
    LODE_CHECK(fixed.domain().shape() == (std::vector<std::size_t>{3}));
    LODE_CHECK_EQUAL(fixed.expr(), f(2, 2) - f(1, 2)*f(2, 1));

    // A rank-0 domain (a single variable as a scalar) gives scalar derivatives
    Function cubic(Tensor<Symbol>(Shape{}, {x}), pow(x, 3));
    LODE_CHECK(cubic.grad().is_scalar());
    LODE_CHECK_EQUAL(cubic.grad().expr(), 3*pow(x, 2));

    return lode::test::report();
}
