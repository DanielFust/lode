/*--------------------------------------------------------------------------------------------------
 * File: benchmark_functions.hpp
 *
 * Description: The functions the benchmark times. Both the benchmark and the program that
 *              generates C++ code for it build them from here, so they are the same functions.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_BENCH_BENCHMARK_FUNCTIONS_HPP
#define LODE_BENCH_BENCHMARK_FUNCTIONS_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "lode/lode.hpp"

namespace lode::bench
{
    // A function to time, the name of its generated code, and a point to time it at
    struct BenchmarkFunction
    {
        std::string         label;
        std::string         name;
        Function            function;
        std::vector<double> point;
    };


   /*-----------------------------------------------------------------------------------------------
    * Function reaction_network(std::size_t species, std::size_t reactions)
    *
    * The right-hand side of a made-up mass-action network: reaction r turns species a and b into
    * c at rate k_r*y_a*y_b, with fixed pseudo-random species and exact rate constants.
    *---------------------------------------------------------------------------------------------*/
    inline Function reaction_network(std::size_t species, std::size_t reactions)
    {
        std::vector<Symbol> y;
        for (std::size_t i = 0; i < species; ++i) y.emplace_back("y" + std::to_string(i));

        std::vector<Expr> rates(species, Expr(0));
        for (std::size_t r = 0; r < reactions; ++r)
        {
            std::size_t a = (7 * r + 1) % species;
            std::size_t b = (11 * r + 3) % species;
            std::size_t c = (13 * r + 5) % species;
            Expr rate = Number(static_cast<std::int64_t>(r % 9 + 1), 10) * y[a] * y[b];
            rates[a] -= rate;
            rates[b] -= rate;
            rates[c] += rate;
        }
        return Function(y, rates);
    }


   /*-----------------------------------------------------------------------------------------------
    * std::vector<BenchmarkFunction> benchmark_functions()
    *
    * Robertson's kinetics (right-hand side and Jacobian), a 30-species network (the same), and
    * the shape functions of a quadratic triangle with their gradients.
    *---------------------------------------------------------------------------------------------*/
    inline std::vector<BenchmarkFunction> benchmark_functions()
    {
        Symbol y1("y1");
        Symbol y2("y2");
        Symbol y3("y3");
        Expr k1 = Number(1, 25);
        Expr k2 = 30000000;
        Expr k3 = 10000;
        Function robertson({y1, y2, y3},
                           {-k1*y1 + k3*y2*y3, k1*y1 - k3*y2*y3 - k2*pow(y2, 2), k2*pow(y2, 2)});
        std::vector<double> state{0.9, 2e-5, 0.1};

        Function network = reaction_network(30, 60);
        std::vector<double> concentrations(30, 0.5);

        Symbol xi("xi");
        Symbol eta("eta");
        Expr l1 = 1 - xi - eta;
        Expr l2 = xi;
        Expr l3 = eta;
        Function shape({xi, eta}, {l1*(2*l1 - 1), l2*(2*l2 - 1), l3*(2*l3 - 1),
                                   4*l1*l2, 4*l2*l3, 4*l3*l1});
        std::vector<double> quadrature_point{0.2, 0.3};

        return {{"Robertson right-hand side", "robertson_rhs", robertson, state},
                {"Robertson Jacobian (3x3)", "robertson_jacobian", robertson.grad(), state},
                {"Network right-hand side (30)", "network_rhs", network, concentrations},
                {"Network Jacobian (30x30)", "network_jacobian", network.grad(), concentrations},
                {"Network Jacobian, sparse (132)", "network_sparse_jacobian",
                 network.sparse_jacobian().values(), concentrations},
                {"Triangle P2 shape functions (6)", "triangle_shape", shape, quadrature_point},
                {"Triangle P2 gradients (6x2)", "triangle_gradients", shape.grad(), quadrature_point}};
    }
}
#endif
