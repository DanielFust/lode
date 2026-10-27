/*--------------------------------------------------------------------------------------------------
 * File: parameters_test.cpp
 *
 * Description: Tests of functions with parameters: inputs whose values are given at each
 *              evaluation and that derivatives leave alone.
 *------------------------------------------------------------------------------------------------*/
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <stdexcept>
#include <string>
#include <vector>

#include "check.hpp"
#include "lode/lode.hpp"

namespace
{
    // Whether two doubles agree to a relative tolerance
    bool close(double a, double b)
    {return std::abs(a - b) <= 1e-13 * std::max({1.0, std::abs(a), std::abs(b)});}

    // Whether two vectors agree entry by entry
    bool close(std::vector<double> const& a, std::vector<double> const& b)
    {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (!close(a[i], b[i])) return false;
        return true;
    }
}


int main()
{
    using lode::Expr;
    using lode::Function;
    using lode::Number;
    using lode::Symbol;
    using Text = std::string;

    // A second-order reaction A + B -> C with an Arrhenius rate k = A*exp(-Ea/(R*T))
    Symbol ca("cA");
    Symbol cb("cB");
    Symbol t("T");
    Symbol a("A");
    Symbol ea("Ea");
    Expr   r = Number(8314, 1000);
    Expr   k = a*exp(-ea/(r*t));
    Function rhs({ca, cb}, {-k*ca*cb, -k*ca*cb}, {t, a, ea});

    LODE_CHECK_EQUAL(rhs.domain().size(), std::size_t{2});
    LODE_CHECK_EQUAL(rhs.parameters().size(), std::size_t{3});
    LODE_CHECK_EQUAL(rhs.input_size(), std::size_t{5});

    // The Jacobian is 2x2: derivatives with respect to the domain only
    Function jacobian = rhs.grad();
    LODE_CHECK(jacobian.shape() == (std::vector<std::size_t>{2, 2}));
    LODE_CHECK_EQUAL(jacobian.parameters().size(), std::size_t{3});
    LODE_CHECK_EQUAL(jacobian[0][1].expr(), -k*ca);

    // Parameter values given separately, or as the last inputs: the same result
    std::vector<double> state{0.8, 0.6};
    std::vector<double> parameters{300.0, 1e7, 5e4};
    std::vector<double> separate(4);
    jacobian.evaluate(state, parameters, separate);
    std::vector<double> together = jacobian.evaluate(std::vector<double>{0.8, 0.6, 300.0, 1e7, 5e4});
    LODE_CHECK(close(separate, together));

    // ... and the same as substituting the values before building
    Function substituted = jacobian.partial({{t, 300.0}, {a, 1e7}, {ea, 5e4}});
    LODE_CHECK(substituted.parameters().empty());
    LODE_CHECK(close(separate, substituted.evaluate(state)));

    // Changing a parameter needs no rebuild
    double k300 = 1e7 * std::exp(-5e4 / (8.314 * 300.0));
    double k350 = 1e7 * std::exp(-5e4 / (8.314 * 350.0));
    std::vector<double> hotter{350.0, 1e7, 5e4};
    std::vector<double> rates(2);
    rhs.evaluate(state, parameters, rates);
    LODE_CHECK(close(rates[0], -k300 * 0.8 * 0.6));
    rhs.evaluate(state, hotter, rates);
    LODE_CHECK(close(rates[0], -k350 * 0.8 * 0.6));

    // Fix the parameters that never change, keep the one that does
    Function by_temperature = jacobian.partial({{a, 1e7}, {ea, 5e4}});
    LODE_CHECK_EQUAL(by_temperature.parameters().size(), std::size_t{1});
    LODE_CHECK_EQUAL(by_temperature.parameters()[0], t);
    std::vector<double> only_t{300.0};
    std::vector<double> fixed(4);
    by_temperature.evaluate(state, only_t, fixed);
    LODE_CHECK(close(fixed, separate));
    LODE_CHECK(by_temperature.instruction_count() <= jacobian.instruction_count());

    // Partial evaluation by argument covers parameters too
    Function at_300 = rhs(ca, cb, 300.0, a, ea);
    LODE_CHECK_EQUAL(at_300.parameters().size(), std::size_t{2});
    LODE_CHECK_EQUAL(at_300.domain().size(), std::size_t{2});

    // A sensitivity: the derivative with respect to a parameter, on request
    Function sensitivity = rhs.diff(ea);
    LODE_CHECK_EQUAL(sensitivity[0].expr(), k*ca*cb/(r*t));

    // Scalar calls take every input in order
    Function scalar({ca}, a*ca, {a});
    LODE_CHECK_EQUAL(scalar(3.0, 2.0), 6.0);
    LODE_CHECK_EQUAL(scalar.grad().to_string(), Text("[A]"));

    // A batch of points sharing the parameter values agrees with pointwise evaluation
    std::vector<double> points;
    for (std::size_t p = 0; p < 50; ++p)
    {
        points.push_back(0.1 + 0.01 * static_cast<double>(p));
        points.push_back(0.9 - 0.01 * static_cast<double>(p));
    }
    std::vector<double> batch(50 * 4);
    jacobian.evaluate_batch(points, parameters, batch);
    bool batch_matches = true;
    for (std::size_t p = 0; p < 50; ++p)
    {
        std::vector<double> point{points[2 * p], points[2 * p + 1]};
        std::vector<double> single(4);
        jacobian.evaluate(point, parameters, single);
        for (std::size_t j = 0; j < 4; ++j)
            batch_matches = batch_matches && close(batch[4 * p + j], single[j]);
    }
    LODE_CHECK(batch_matches);

    // Errors
    LODE_CHECK_THROWS(Function({ca}, ca*a, {ca}), std::invalid_argument);
    LODE_CHECK_THROWS(Function({ca}, ca*a*t, {a}), std::invalid_argument);
    std::vector<double> too_few{300.0};
    LODE_CHECK_THROWS(jacobian.evaluate(state, too_few, separate), std::invalid_argument);
    LODE_CHECK_THROWS(jacobian.evaluate_batch(points, too_few, batch), std::invalid_argument);
    try
    {
        Function({ca}, ca*a*t, {a});
    }
    catch (std::invalid_argument const& error)
    {
        Text expected = "Function: 'T' is not in the domain (cA) or the parameters (A)";
        LODE_CHECK_EQUAL(Text(error.what()), expected);
    }

    return lode::test::report();
}
