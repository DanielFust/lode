/*--------------------------------------------------------------------------------------------------
 * File: 08_batch_evaluation.cpp
 *
 * Description: Example 8: evaluating many points at once.
 *
 *              evaluate_batch evaluates a function at many points in one call. It runs each
 *              instruction over a block of points, which is several times faster per point than
 *              evaluating the points one at a time.
 *
 *              Build and run from the repository root:
 *
 *                  cmake -S . -B build && cmake --build build
 *                  ./build/examples/08_batch_evaluation
 *------------------------------------------------------------------------------------------------*/
#include <chrono>
#include <cstddef>
#include <iostream>
#include <span>
#include <vector>

#include "lode/lode.hpp"

using namespace lode;

int main()
{
    Symbol x("x");
    Symbol y("y");

    // A function and its gradient, together: three outputs
    Expr     f = sin(x)*exp(-y) + x*pow(y, 2);
    Function values({x, y}, {f, diff(f, x), diff(f, y)});

    /*----------------------------------- Tabulating on a grid -----------------------------------*/

    // Points are stored one after another: x0, y0, x1, y1, ... and the outputs likewise,
    // three per point
    std::size_t const n = 101;
    std::vector<double> points;
    for (std::size_t i = 0; i < n; ++i)
        for (std::size_t j = 0; j < n; ++j)
        {
            points.push_back(0.03 * static_cast<double>(i));
            points.push_back(0.02 * static_cast<double>(j));
        }
    std::vector<double> outputs(n * n * values.size());
    values.evaluate_batch(points, outputs);

    // Point p's inputs start at points[2*p] and its outputs at outputs[3*p]
    std::size_t const p = 5 * n + 7;
    std::cout << "points evaluated: " << n * n << "\n";
    std::cout << "at (x, y) = (" << points[2 * p] << ", " << points[2 * p + 1] << "): f = "
              << outputs[3 * p] << ", df/dx = " << outputs[3 * p + 1] << ", df/dy = "
              << outputs[3 * p + 2] << "\n";

    /*------------------------------- One at a time versus a batch -------------------------------*/

    using Clock = std::chrono::steady_clock;
    Clock::time_point start = Clock::now();
    for (std::size_t p = 0; p < n * n; ++p)
    {
        std::span<double const> point(points.data() + 2 * p, 2);
        std::span<double>       out(outputs.data() + 3 * p, 3);
        values.evaluate(point, out);
    }
    Clock::time_point middle = Clock::now();
    values.evaluate_batch(points, outputs);
    Clock::time_point end = Clock::now();

    // Timings depend on the build: configure with -DCMAKE_BUILD_TYPE=Release to see real speed
    double count         = static_cast<double>(n * n);
    double one_at_a_time = std::chrono::duration<double, std::nano>(middle - start).count();
    double batch         = std::chrono::duration<double, std::nano>(end - middle).count();
    std::cout << "nanoseconds per point: one at a time " << one_at_a_time / count
              << ", batch " << batch / count << "\n";

    return 0;
}
