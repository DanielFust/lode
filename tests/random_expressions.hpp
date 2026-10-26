/*--------------------------------------------------------------------------------------------------
 * File: random_expressions.hpp
 *
 * Description: A generator of small random expressions for randomized tests.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_TESTS_RANDOM_EXPRESSIONS_HPP
#define LODE_TESTS_RANDOM_EXPRESSIONS_HPP

#include <cstddef>
#include <cstdint>
#include <random>
#include <utility>
#include <vector>

#include "lode/lode.hpp"

namespace lode::test
{
   /*-----------------------------------------------------------------------------------------------
    * class RandomExpressions
    *
    * Generates small random expressions in a few symbols, with exact coefficients, integer
    * exponents and (optionally) elementary functions, from a fixed seed so that failures repeat.
    *---------------------------------------------------------------------------------------------*/
    class RandomExpressions
    {
      private:
        std::mt19937        engine_;
        std::vector<Symbol> symbols_;
        bool                functions_; // Whether to apply exp, log, sin and cos

      public:
        RandomExpressions(std::uint32_t seed, std::vector<Symbol> symbols, bool functions)
            : engine_(seed), symbols_(std::move(symbols)), functions_(functions) {}

        // A random expression with at most the given depth of operations
        Expr next(int depth);

        // A uniformly random integer in [low, high]
        int uniform(int low, int high)
        {return std::uniform_int_distribution<int>(low, high)(this->engine_);}

        // A uniformly random real in [low, high)
        double uniform_real(double low, double high)
        {return std::uniform_real_distribution<double>(low, high)(this->engine_);}
    };


   /*-----------------------------------------------------------------------------------------------
    * Expr RandomExpressions::next(int depth)
    *
    * A random leaf (symbol, integer or fraction) at depth 0. Otherwise a random sum, difference,
    * product, quotient or integer power of smaller random expressions, or a function of one.
    * Operations that would divide by zero or take log(0) are replaced by a fresh expression.
    *---------------------------------------------------------------------------------------------*/
    inline Expr RandomExpressions::next(int depth)
    {
        if (depth == 0 || this->uniform(0, 3) == 0)
        {
            switch (this->uniform(0, 3))
            {
                case 0:
                    return Expr(this->uniform(-3, 3));
                case 1:
                    return Expr(Number(this->uniform(-3, 3), this->uniform(1, 4)));
                default:
                    return this->symbols_[static_cast<std::size_t>(this->uniform(0, 2))];
            }
        }

        Expr a = this->next(depth - 1);
        int  operation = this->uniform(0, this->functions_ ? 6 : 4);
        if (operation == 5)
        {
            switch (this->uniform(0, 3))
            {
                case 0:
                    return exp(a);
                case 1:
                    if (a == Expr(0)) return this->next(depth);
                    return log(a);
                case 2:
                    return sin(a);
                default:
                    return cos(a);
            }
        }

        Expr b = this->next(depth - 1);
        switch (operation)
        {
            case 0:
                return a + b;
            case 1:
                return a - b;
            case 2:
                return a*b;
            case 3:
                if (b == Expr(0)) return this->next(depth);
                return a/b;
            default:
            {
                int exponent = this->uniform(-2, 3);
                if (a == Expr(0) && exponent < 0) return this->next(depth);
                return pow(a, exponent);
            }
        }
    }
}
#endif
