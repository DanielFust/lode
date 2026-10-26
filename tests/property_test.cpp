/*--------------------------------------------------------------------------------------------------
 * File: property_test.cpp
 *
 * Description: Randomized tests of the canonical form. Random expressions are combined in
 *              different orders and groupings, and the results must be equal: the algebraic
 *              laws hold structurally, not just in value.
 *------------------------------------------------------------------------------------------------*/
#include <cstdint>
#include <iostream>
#include <random>
#include <stdexcept>
#include <vector>

#include "check.hpp"
#include "lode/lode.hpp"

namespace
{
    using lode::Expr;
    using lode::Number;
    using lode::Symbol;

   /*-----------------------------------------------------------------------------------------------
    * class RandomExpressions
    *
    * Generates small random expressions in a few symbols, with exact coefficients and integer
    * exponents, from a fixed seed so that failures repeat.
    *---------------------------------------------------------------------------------------------*/
    class RandomExpressions
    {
      private:
        std::mt19937        engine_;
        std::vector<Symbol> symbols_;

      public:
        RandomExpressions(std::uint32_t seed, std::vector<Symbol> symbols)
            : engine_(seed), symbols_(std::move(symbols)) {}

        // A random expression with at most the given depth of operations
        Expr next(int depth);

      private:
        // A uniformly random integer in [low, high]
        int uniform(int low, int high)
        {return std::uniform_int_distribution<int>(low, high)(this->engine_);}
    };


   /*-----------------------------------------------------------------------------------------------
    * Expr RandomExpressions::next(int depth)
    *
    * A random leaf (symbol, integer or fraction) at depth 0, otherwise a random sum, difference,
    * product, quotient or integer power of smaller random expressions. A quotient whose
    * denominator is zero is replaced by a fresh expression.
    *---------------------------------------------------------------------------------------------*/
    Expr RandomExpressions::next(int depth)
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
        Expr b = this->next(depth - 1);
        switch (this->uniform(0, 4))
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


int main()
{
    Symbol x("x");
    Symbol y("y");
    Symbol z("z");
    RandomExpressions random(20261026, {x, y, z});

    int const TRIALS = 2000;
    for (int trial = 0; trial < TRIALS; ++trial)
    {
        Expr e = random.next(3);
        Expr f = random.next(3);
        Expr g = random.next(3);

        // Commutative, associative and distributive laws (numbers distribute over sums)
        LODE_CHECK_EQUAL(e + f, f + e);
        LODE_CHECK_EQUAL(e*f, f*e);
        LODE_CHECK_EQUAL((e + f) + g, e + (f + g));
        LODE_CHECK_EQUAL((e*f)*g, e*(f*g));
        LODE_CHECK_EQUAL(3*(e + f), 3*e + 3*f);

        // Inverses
        LODE_CHECK_EQUAL(e - e, Expr(0));
        LODE_CHECK_EQUAL((e + f) - f, e);
        if (f != Expr(0))
        {
            LODE_CHECK_EQUAL(e*f/f, e);
            LODE_CHECK_EQUAL(f/f, Expr(1));
        }

        // Integer powers
        LODE_CHECK_EQUAL(pow(e*f, 2), pow(e, 2)*pow(f, 2));
        LODE_CHECK_EQUAL(pow(e, 2)*e, pow(e, 3));

        // Equal expressions hash equally
        LODE_CHECK_EQUAL((e + f).hash(), (f + e).hash());
    }

    return lode::test::report();
}
