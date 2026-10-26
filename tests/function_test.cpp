/*--------------------------------------------------------------------------------------------------
 * File: function_test.cpp
 *
 * Description: Tests of Function: construction, compiled evaluation (one point and batches),
 *              derivatives, partial evaluation, and the compiled programs themselves.
 *------------------------------------------------------------------------------------------------*/
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "check.hpp"
#include "lode/lode.hpp"
#include "random_expressions.hpp"

namespace
{
    using lode::Expr;
    using lode::Function;
    using lode::Number;
    using lode::Symbol;
    using Complex = std::complex<double>;
    using Text    = std::string;

    // Whether two doubles agree to a relative tolerance, or are both NaN
    bool close(double a, double b, double tolerance = 1e-14)
    {
        if (std::isnan(a) || std::isnan(b)) return std::isnan(a) && std::isnan(b);
        if (std::isinf(a) || std::isinf(b)) return a == b;
        return std::abs(a - b) <= tolerance * std::max({1.0, std::abs(a), std::abs(b)});
    }


   /*-----------------------------------------------------------------------------------------------
    * void check_against_reference(lode::test::RandomExpressions& random, int trials)
    *
    * Compiles random expressions and checks that the compiled evaluator agrees with the
    * reference evaluator, and that compiled gradients agree with complex-step derivatives of the
    * compiled function (which exercises the compiler with complex numbers).
    *---------------------------------------------------------------------------------------------*/
    void check_against_reference(lode::test::RandomExpressions& random, int trials,
                                 std::vector<Symbol> const& variables)
    {
        double const STEP = 1e-20;
        for (int trial = 0; trial < trials; ++trial)
        {
            Expr     e = random.next(3);
            Function f(variables, e);

            std::vector<double>  point;
            lode::Values<double> values;
            for (Symbol const& variable : variables)
            {
                point.push_back(random.uniform_real(0.5, 2.0));
                values.emplace_back(variable, point.back());
            }

            double compiled  = f.evaluate(point).front();
            double reference = lode::evaluate(e, values);
            LODE_CHECK(close(compiled, reference, 1e-12));

            // Gradient against complex step, where the function is finite
            if (!std::isfinite(compiled)) continue;
            std::vector<double> gradient = f.grad().evaluate(point);
            for (std::size_t i = 0; i < variables.size(); ++i)
            {
                if (!std::isfinite(gradient[i])) continue;
                std::vector<Complex> stepped(point.begin(), point.end());
                stepped[i] += Complex(0.0, STEP);
                Complex value;
                f.evaluate<Complex>(stepped, std::span<Complex>(&value, 1));
                LODE_CHECK(close(gradient[i], value.imag() / STEP, 1e-9));
            }
        }
    }
}


int main()
{
    Symbol x("x");
    Symbol y("y");
    Symbol z("z");
    Symbol t("t");

    // The nablaxx demo, in its original style
    Function f(x, y, z);
    f = x*x*2 + 1 + y + x + x*y*z;
    LODE_CHECK_EQUAL(f.to_string(), Text("x + 2*x^2 + y + x*y*z + 1"));
    LODE_CHECK_EQUAL(f(1.0, 2.0, 3.0), 12.0);
    LODE_CHECK_EQUAL(f(1, 2, 3), 12.0);
    LODE_CHECK(f.is_scalar());
    LODE_CHECK_EQUAL(f.input_size(), std::size_t{3});

    // Gradient and Hessian
    Function grad_f = f.grad();
    LODE_CHECK_EQUAL(grad_f.to_string(), Text("[4*x + y*z + 1, x*z + 1, x*y]"));
    std::vector<double> gradient_at = grad_f.evaluate(std::vector<double>{1.0, 2.0, 3.0});
    LODE_CHECK(gradient_at == (std::vector<double>{11.0, 4.0, 2.0}));
    LODE_CHECK_EQUAL(grad_f[1].expr(), x*z + 1);
    Function hessian_f = f.hessian();
    LODE_CHECK_EQUAL(hessian_f.to_string(), Text("[[4, z, y], [z, 0, x], [y, x, 0]]"));
    LODE_CHECK(hessian_f.shape() == (std::vector<std::size_t>{3, 3}));
    LODE_CHECK_EQUAL(hessian_f[2].to_string(), Text("[y, x, 0]"));
    LODE_CHECK_EQUAL(f.diff(y).expr(), x*z + 1);

    // Robertson's stiff chemical kinetics: right-hand side and exact Jacobian
    Symbol y1("y1");
    Symbol y2("y2");
    Symbol y3("y3");
    Expr k1 = Number(1, 25);
    Expr k2 = 30000000;
    Expr k3 = 10000;
    Function rhs({y1, y2, y3}, {-k1*y1 + k3*y2*y3,
                                k1*y1 - k3*y2*y3 - k2*pow(y2, 2),
                                k2*pow(y2, 2)});
    Function jacobian = rhs.grad();
    LODE_CHECK(jacobian.shape() == (std::vector<std::size_t>{3, 3}));
    std::vector<double> state{0.9, 2e-5, 0.1};
    std::vector<double> computed = jacobian.evaluate(state);
    std::vector<double> by_hand{-0.04, 1e4 * state[2], 1e4 * state[1],
                                0.04, -1e4 * state[2] - 6e7 * state[1], -1e4 * state[1],
                                0.0, 6e7 * state[1], 0.0};
    bool jacobian_matches = computed.size() == by_hand.size();
    for (std::size_t i = 0; jacobian_matches && i < by_hand.size(); ++i)
        jacobian_matches = close(computed[i], by_hand[i]);
    LODE_CHECK(jacobian_matches);

    // Batch evaluation agrees with pointwise evaluation (100 points: not a multiple of a block)
    std::size_t const POINTS = 100;
    std::vector<double> inputs;
    for (std::size_t p = 0; p < POINTS; ++p)
    {
        inputs.push_back(1.0 - 0.001 * static_cast<double>(p));
        inputs.push_back(1e-5 * static_cast<double>(p));
        inputs.push_back(0.001 * static_cast<double>(p));
    }
    std::vector<double> batch(POINTS * 9);
    jacobian.evaluate_batch(inputs, batch);
    bool batch_matches = true;
    for (std::size_t p = 0; p < POINTS; ++p)
    {
        std::span<double const> point = std::span<double const>(inputs).subspan(3 * p, 3);
        std::vector<double>     single = jacobian.evaluate(point);
        for (std::size_t j = 0; j < 9; ++j)
            batch_matches = batch_matches && close(batch[9 * p + j], single[j]);
    }
    LODE_CHECK(batch_matches);

    // Partial evaluation: fixing y gives a function of (x, z), with constants folded
    Function g(x, y, z);
    g = x*y + pow(y, 2) + z;
    Function g_fixed = g(x, 2, z);
    LODE_CHECK_EQUAL(g_fixed.input_size(), std::size_t{2});
    LODE_CHECK_EQUAL(g_fixed.domain()[0], x);
    LODE_CHECK_EQUAL(g_fixed.domain()[1], z);
    LODE_CHECK_EQUAL(g_fixed.expr(), 2*x + z + 4);
    LODE_CHECK_EQUAL(g_fixed(1.5, 3.0), g(1.5, 2.0, 3.0));
    LODE_CHECK(g_fixed.instruction_count() < g.instruction_count());
    LODE_CHECK_EQUAL(g.partial({{y, 2}}).expr(), g_fixed.expr());
    LODE_CHECK(g(x, 2.5, z).expr() != g(x, Number(5, 2), z).expr());

    // Renaming, reordering and reparametrizing
    Function g_swapped = g(z, y, x);
    LODE_CHECK_EQUAL(g_swapped.domain()[0], z);
    LODE_CHECK_EQUAL(g_swapped.expr(), z*y + pow(y, 2) + x);
    Function g_line = g(2*t, y, 1 - t);
    LODE_CHECK_EQUAL(g_line.input_size(), std::size_t{2});
    LODE_CHECK_EQUAL(g_line.domain()[0], t);
    LODE_CHECK_EQUAL(g_line.expr(), 2*t*y + pow(y, 2) - t + 1);
    LODE_CHECK_EQUAL(g.partial({{z, t}}).domain()[2], t);

    // Compiled programs share work: (x + y) is computed once, and (x + y)^2 serves (x + y)^3
    Function powers({x, y}, pow(x + y, 2) + pow(x + y, 3));
    LODE_CHECK_EQUAL(powers.instruction_count(), std::size_t{5});
    Function shared({x, y}, std::vector<Expr>{sin(x*y), cos(x*y)});
    LODE_CHECK_EQUAL(shared.instruction_count(), std::size_t{5});
    Function constant({x}, Expr(7));
    LODE_CHECK_EQUAL(constant(3.0), 7.0);
    LODE_CHECK_EQUAL(constant.listing(), Text("out[0] = 7.0\n"));
    Function long_chain({x}, pow(x, 10) + pow(x, 9) + pow(x, 8) + pow(x, 7));
    LODE_CHECK(long_chain.register_count() < long_chain.instruction_count());

    // Other number types
    Function cube({x}, pow(x, 3) - 2*x);
    float as_float = 0.0f;
    cube.evaluate<float>(std::vector<float>{2.0f}, std::span<float>(&as_float, 1));
    LODE_CHECK_EQUAL(as_float, 4.0f);

    // Errors
    LODE_CHECK_THROWS(Function({x}, x*y), std::invalid_argument);
    LODE_CHECK_THROWS(Function({x, x}, Expr(x)), std::invalid_argument);
    LODE_CHECK_THROWS(Function({x}, std::vector<Expr>{x, x}, {3}), std::invalid_argument);
    LODE_CHECK_THROWS(f(1.0, 2.0), std::invalid_argument);
    LODE_CHECK_THROWS(grad_f(1.0, 2.0, 3.0), std::logic_error);
    LODE_CHECK_THROWS(grad_f.expr(), std::logic_error);
    LODE_CHECK_THROWS(grad_f[3], std::out_of_range);
    LODE_CHECK_THROWS(f[0], std::logic_error);
    LODE_CHECK_THROWS(f.evaluate(std::vector<double>{1.0}), std::invalid_argument);
    std::vector<double> four(4);
    std::vector<double> nine(9);
    LODE_CHECK_THROWS(jacobian.evaluate_batch(four, nine), std::invalid_argument);
    try
    {
        Function({x}, x*y);
    }
    catch (std::invalid_argument const& error)
    {
        LODE_CHECK_EQUAL(Text(error.what()), Text("Function: 'y' is not in the domain (x)"));
    }

    // Random expressions: compiled against the reference evaluator and complex-step gradients
    lode::test::RandomExpressions random(20261029, {x, y, z}, true);
    check_against_reference(random, 1000, {x, y, z});

    return lode::test::report();
}
