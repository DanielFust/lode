/*--------------------------------------------------------------------------------------------------
 * File: benchmark.cpp
 *
 * Description: Timing of compiled evaluation against the reference evaluator and hand-written
 *              C++, on a chemistry Jacobian, a larger reaction network and finite-element shape
 *              function gradients. Build with -DLODE_BUILD_BENCHMARKS=ON in Release mode.
 *------------------------------------------------------------------------------------------------*/
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <functional>
#include <span>
#include <string>
#include <vector>

#include "lode/lode.hpp"

namespace
{
    using lode::Expr;
    using lode::Function;
    using lode::Number;
    using lode::Symbol;
    using Clock = std::chrono::steady_clock;

    // Result accumulated from every evaluation, so the compiler cannot skip the work
    volatile double sink = 0.0;


   /*-----------------------------------------------------------------------------------------------
    * double time_per_point(std::function<void()> const& work, std::size_t points)
    *
    * Nanoseconds per point: runs work (which evaluates `points` points) repeatedly for at least
    * 0.2 seconds and divides.
    *---------------------------------------------------------------------------------------------*/
    double time_per_point(std::function<void()> const& work, std::size_t points)
    {
        work();
        std::size_t repeats = 0;
        Clock::time_point start = Clock::now();
        Clock::duration   elapsed{};
        while (elapsed < std::chrono::milliseconds(200))
        {
            work();
            ++repeats;
            elapsed = Clock::now() - start;
        }
        double nanoseconds = std::chrono::duration<double, std::nano>(elapsed).count();
        return nanoseconds / static_cast<double>(repeats * points);
    }


   /*-----------------------------------------------------------------------------------------------
    * void report(std::string const& name, Function const& function, std::vector<double> const& point,
    *             std::function<void(double const*, double*)> const& by_hand)
    *
    * Times one function four ways (reference evaluator, compiled one point at a time, compiled
    * in batches, and hand-written code if given) and prints a row of the table.
    *---------------------------------------------------------------------------------------------*/
    void report(std::string const& name, Function const& function, std::vector<double> const& point,
                std::function<void(double const*, double*)> const& by_hand)
    {
        std::size_t const POINTS = 1024;
        std::size_t const n_in   = function.input_size();
        std::size_t const n_out  = function.size();

        // Many copies of the point, slightly varied
        std::vector<double> inputs(POINTS * n_in);
        for (std::size_t p = 0; p < POINTS; ++p)
            for (std::size_t i = 0; i < n_in; ++i)
                inputs[p * n_in + i] = point[i] * (1.0 + 1e-6 * static_cast<double>(p));
        std::vector<double> outputs(POINTS * n_out);

        // Reference: the tree-walking evaluator, on a few points (it is slow)
        std::size_t const FEW = 16;
        lode::Values<double> values;
        for (std::size_t i = 0; i < n_in; ++i) values.emplace_back(function.domain()[i], 0.0);
        double reference = time_per_point([&]()
        {
            for (std::size_t p = 0; p < FEW; ++p)
            {
                for (std::size_t i = 0; i < n_in; ++i) values[i].second = inputs[p * n_in + i];
                for (Expr const& output : function.outputs())
                    sink = sink + lode::evaluate(output, values);
            }
        }, FEW);

        double single = time_per_point([&]()
        {
            for (std::size_t p = 0; p < POINTS; ++p)
            {
                std::span<double const> in(inputs.data() + p * n_in, n_in);
                std::span<double>       out(outputs.data() + p * n_out, n_out);
                function.evaluate(in, out);
            }
            sink = sink + outputs.back();
        }, POINTS);

        double batch = time_per_point([&]()
        {
            function.evaluate_batch(inputs, outputs);
            sink = sink + outputs.back();
        }, POINTS);

        double hand = 0.0;
        if (by_hand)
        {
            hand = time_per_point([&]()
            {
                for (std::size_t p = 0; p < POINTS; ++p)
                    by_hand(inputs.data() + p * n_in, outputs.data() + p * n_out);
                sink = sink + outputs.back();
            }, POINTS);
        }

        std::printf("%-34s %6zu %6zu %10.1f %10.1f %10.1f", name.c_str(), function.instruction_count(),
                    function.register_count(), reference, single, batch);
        if (by_hand) std::printf(" %10.1f", hand);
        std::printf("\n");
    }


   /*-----------------------------------------------------------------------------------------------
    * Function reaction_network(std::size_t species, std::size_t reactions)
    *
    * The right-hand side of a made-up mass-action network: reaction r turns species a and b into
    * c at rate k_r*y_a*y_b, with fixed pseudo-random species and exact rate constants.
    *---------------------------------------------------------------------------------------------*/
    Function reaction_network(std::size_t species, std::size_t reactions)
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
}


int main()
{
    std::printf("Nanoseconds per point (one evaluation of every output)\n\n");
    std::printf("%-34s %6s %6s %10s %10s %10s %10s\n", "", "instr", "regs", "reference", "compiled",
                "batch", "by hand");

    // Robertson's stiff kinetics: right-hand side and Jacobian
    Symbol y1("y1");
    Symbol y2("y2");
    Symbol y3("y3");
    Expr k1 = Number(1, 25);
    Expr k2 = 30000000;
    Expr k3 = 10000;
    Function rhs({y1, y2, y3}, {-k1*y1 + k3*y2*y3, k1*y1 - k3*y2*y3 - k2*pow(y2, 2), k2*pow(y2, 2)});
    std::vector<double> state{0.9, 2e-5, 0.1};
    report("Robertson right-hand side", rhs, state, [](double const* y, double* f)
    {
        f[0] = -0.04 * y[0] + 1e4 * y[1] * y[2];
        f[1] = 0.04 * y[0] - 1e4 * y[1] * y[2] - 3e7 * y[1] * y[1];
        f[2] = 3e7 * y[1] * y[1];
    });
    report("Robertson Jacobian (3x3)", rhs.grad(), state, [](double const* y, double* j)
    {
        j[0] = -0.04; j[1] = 1e4 * y[2];              j[2] = 1e4 * y[1];
        j[3] = 0.04;  j[4] = -1e4 * y[2] - 6e7 * y[1]; j[5] = -1e4 * y[1];
        j[6] = 0.0;   j[7] = 6e7 * y[1];              j[8] = 0.0;
    });

    // A larger network: 30 species, 60 reactions
    Function network = reaction_network(30, 60);
    std::vector<double> concentrations(30, 0.5);
    report("Network right-hand side (30)", network, concentrations, nullptr);
    report("Network Jacobian (30x30)", network.grad(), concentrations, nullptr);

    // Quadratic triangle (6 nodes): shape functions and their gradients
    Symbol xi("xi");
    Symbol eta("eta");
    Expr l1 = 1 - xi - eta;
    Expr l2 = xi;
    Expr l3 = eta;
    Function shape({xi, eta}, {l1*(2*l1 - 1), l2*(2*l2 - 1), l3*(2*l3 - 1),
                               4*l1*l2, 4*l2*l3, 4*l3*l1});
    std::vector<double> quadrature_point{0.2, 0.3};
    report("Triangle P2 shape functions (6)", shape, quadrature_point, [](double const* p, double* n)
    {
        double a = 1.0 - p[0] - p[1];
        double b = p[0];
        double c = p[1];
        n[0] = a * (2.0 * a - 1.0); n[1] = b * (2.0 * b - 1.0); n[2] = c * (2.0 * c - 1.0);
        n[3] = 4.0 * a * b;         n[4] = 4.0 * b * c;         n[5] = 4.0 * c * a;
    });
    report("Triangle P2 gradients (6x2)", shape.grad(), quadrature_point,
           [](double const* p, double* g)
    {
        double a = 1.0 - p[0] - p[1];
        double b = p[0];
        double c = p[1];
        g[0]  = 1.0 - 4.0 * a;      g[1]  = 1.0 - 4.0 * a;
        g[2]  = 4.0 * b - 1.0;      g[3]  = 0.0;
        g[4]  = 0.0;                g[5]  = 4.0 * c - 1.0;
        g[6]  = 4.0 * (a - b);      g[7]  = -4.0 * b;
        g[8]  = 4.0 * c;            g[9]  = 4.0 * b;
        g[10] = -4.0 * c;           g[11] = 4.0 * (a - c);
    });

    return sink == 12345.0 ? 1 : 0;
}
