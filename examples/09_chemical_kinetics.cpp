/*--------------------------------------------------------------------------------------------------
 * File: 09_chemical_kinetics.cpp
 *
 * Description: Example 9, an application: stiff chemical kinetics.
 *
 *              Robertson's reactions are a classic stiff problem:
 *
 *                  A     -> B          rate constant k1 = 0.04
 *                  B + B -> C + B      rate constant k2 = 3e7
 *                  B + C -> A + C      rate constant k3 = 1e4
 *
 *              The rates differ by nine orders of magnitude, so an explicit method needs tiny
 *              steps for the whole run. An implicit method can take large steps, but it solves a
 *              nonlinear system at every step, which needs the Jacobian of the right-hand side.
 *              Here LODE builds the right-hand side and its exact Jacobian, with the rate
 *              constants as parameters, and the program integrates with backward Euler and
 *              Newton's method.
 *
 *              Build and run from the repository root:
 *
 *                  cmake -S . -B build && cmake --build build
 *                  ./build/examples/09_chemical_kinetics
 *------------------------------------------------------------------------------------------------*/
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <iostream>
#include <utility>
#include <vector>

#include "lode/lode.hpp"

using namespace lode;

namespace
{
    using State  = std::array<double, 3>;
    using Matrix = std::array<double, 9>;   // 3x3, row by row


   /*-----------------------------------------------------------------------------------------------
    * State solve(Matrix a, State b)
    *
    * Solves the 3x3 linear system a*x = b by Gaussian elimination with partial pivoting.
    *---------------------------------------------------------------------------------------------*/
    State solve(Matrix a, State b)
    {
        for (std::size_t column = 0; column < 3; ++column)
        {
            // Bring the largest remaining entry of the column to the diagonal
            std::size_t pivot = column;
            for (std::size_t row = column + 1; row < 3; ++row)
                if (std::abs(a[3 * row + column]) > std::abs(a[3 * pivot + column])) pivot = row;
            for (std::size_t k = 0; k < 3; ++k) std::swap(a[3 * column + k], a[3 * pivot + k]);
            std::swap(b[column], b[pivot]);

            // Eliminate the column below the diagonal
            for (std::size_t row = column + 1; row < 3; ++row)
            {
                double factor = a[3 * row + column] / a[3 * column + column];
                for (std::size_t k = column; k < 3; ++k) a[3 * row + k] -= factor * a[3 * column + k];
                b[row] -= factor * b[column];
            }
        }

        // Back substitution
        State x{};
        for (std::size_t row = 3; row-- > 0;)
        {
            double sum = b[row];
            for (std::size_t k = row + 1; k < 3; ++k) sum -= a[3 * row + k] * x[k];
            x[row] = sum / a[3 * row + row];
        }
        return x;
    }
}


int main()
{
    /*---------------------------------------- The model -----------------------------------------*/

    // Concentrations of A, B and C are the variables; the rate constants are parameters
    Symbol y1("y1");
    Symbol y2("y2");
    Symbol y3("y3");
    Symbol k1("k1");
    Symbol k2("k2");
    Symbol k3("k3");

    Function rhs({y1, y2, y3},
                 {-k1*y1 + k3*y2*y3,
                  k1*y1 - k3*y2*y3 - k2*pow(y2, 2),
                  k2*pow(y2, 2)},
                 {k1, k2, k3});
    Function jacobian = rhs.grad();

    std::cout << "dy/dt = " << rhs << "\n";
    std::cout << "J     = " << jacobian << "\n";
    std::cout << "compiled: right-hand side " << rhs.instruction_count() << " instructions, Jacobian "
              << jacobian.instruction_count() << "\n\n";

    std::vector<double> rate_constants{0.04, 3e7, 1e4};

    /*--------------------------- Backward Euler with Newton's method ----------------------------*/

    // Each step solves G(y) = y - y_old - h*f(y) = 0 for the new state y by Newton's method,
    // whose matrix is G'(y) = I - h*J(y). The step size h starts tiny, for the fast initial
    // transient, and grows by 1% per step as the solution settles.
    State  y{1.0, 0.0, 0.0};
    double t = 0.0;
    double h = 1e-6;
    std::size_t steps = 0;
    std::size_t jacobians = 0;

    std::printf("%10s %14s %14s %14s %18s\n", "t", "y1", "y2", "y3", "y1 + y2 + y3 - 1");
    for (double t_out : {0.4, 4.0, 40.0, 400.0, 4000.0, 40000.0})
    {
        while (t < t_out)
        {
            double step = std::min(h, t_out - t);
            State  next = y;
            for (int iteration = 0; iteration < 20; ++iteration)
            {
                State  f{};
                Matrix j{};
                rhs.evaluate(next, rate_constants, f);
                jacobian.evaluate(next, rate_constants, j);
                ++jacobians;

                State  g{};
                Matrix dg{};
                for (std::size_t r = 0; r < 3; ++r)
                {
                    g[r] = next[r] - y[r] - step * f[r];
                    for (std::size_t c = 0; c < 3; ++c)
                        dg[3 * r + c] = (r == c ? 1.0 : 0.0) - step * j[3 * r + c];
                }

                State  delta  = solve(dg, g);
                double change = 0.0;
                for (std::size_t r = 0; r < 3; ++r)
                {
                    next[r] -= delta[r];
                    change = std::max(change, std::abs(delta[r]));
                }
                if (change < 1e-14) break;
            }
            y  = next;
            t += step;
            h *= 1.01;
            ++steps;
        }
        double drift = y[0] + y[1] + y[2] - 1.0;
        std::printf("%10g %14.8f %14.6e %14.8f %18.2e\n", t, y[0], y[1], y[2], drift);
    }

    std::cout << "\n" << steps << " steps, " << jacobians << " Jacobian evaluations\n";
    std::cout << "reference at t = 40 (Hairer and Wanner): y1 = 0.71582707, y2 = 9.185535e-06, "
              << "y3 = 0.28416375\n";

    return 0;
}
