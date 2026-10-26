/*--------------------------------------------------------------------------------------------------
 * File: number_test.cpp
 *
 * Description: Tests of Number: exact rational arithmetic, the fallback to reals, exact powers,
 *              ordering and text.
 *------------------------------------------------------------------------------------------------*/
#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

#include "check.hpp"
#include "lode/lode.hpp"

int main()
{
    using lode::Number;

    // Rationals are kept in lowest terms with a positive denominator
    LODE_CHECK_EQUAL(Number(6, -4), Number(-3, 2));
    LODE_CHECK_EQUAL(Number(6, -4).numerator(), -3);
    LODE_CHECK_EQUAL(Number(6, -4).denominator(), 2);

    // Arithmetic on rationals is exact
    LODE_CHECK_EQUAL(Number(1, 3) + Number(1, 6), Number(1, 2));
    LODE_CHECK_EQUAL(Number(2, 3) * Number(3, 4), Number(1, 2));
    LODE_CHECK_EQUAL(Number(1) / Number(3) * 3, Number(1));
    LODE_CHECK((Number(1, 2) - Number(1, 2)).is_zero());
    LODE_CHECK_EQUAL(-Number(1, 2), Number(-1, 2));

    // A rational and a real differ even with equal values; mixing them gives a real
    LODE_CHECK(Number(2) != Number(2.0));
    LODE_CHECK((Number(1, 2) + 0.25).is_real());
    LODE_CHECK_EQUAL((Number(1, 2) + 0.25).to_double(), 0.75);

    // A result that overflows 64 bits becomes a real
    Number largest = std::numeric_limits<std::int64_t>::max();
    LODE_CHECK((largest + 0).is_rational());
    LODE_CHECK((largest + 1).is_real());
    LODE_CHECK((largest * largest).is_real());
    LODE_CHECK(Number(std::numeric_limits<std::uint64_t>::max()).is_real());

    // Invalid numbers and operations throw
    LODE_CHECK_THROWS(Number(1, 0), std::domain_error);
    LODE_CHECK_THROWS(Number(1) / Number(0), std::domain_error);
    LODE_CHECK_THROWS(Number(1.0) / Number(0.0), std::domain_error);
    LODE_CHECK_THROWS(Number(std::nan("")), std::domain_error);
    LODE_CHECK_THROWS(Number(1.5).numerator(), std::logic_error);

    // Powers are exact when the result is rational, and absent when it is not
    LODE_CHECK_EQUAL(exact_pow(Number(2, 3), Number(3)).value(), Number(8, 27));
    LODE_CHECK_EQUAL(exact_pow(Number(2), Number(-2)).value(), Number(1, 4));
    LODE_CHECK_EQUAL(exact_pow(Number(4, 9), Number(1, 2)).value(), Number(2, 3));
    LODE_CHECK_EQUAL(exact_pow(Number(-8), Number(1, 3)).value(), Number(-2));
    LODE_CHECK_EQUAL(exact_pow(Number(8), Number(2, 3)).value(), Number(4));
    LODE_CHECK_EQUAL(exact_pow(Number(0), Number(0)).value(), Number(1));
    LODE_CHECK(!exact_pow(Number(2), Number(1, 2)).has_value());
    LODE_CHECK(!exact_pow(Number(-4), Number(1, 2)).has_value());
    LODE_CHECK(!exact_pow(Number(-4.0), Number(0.5)).has_value());
    LODE_CHECK(exact_pow(Number(2), Number(100)).value().is_real());
    LODE_CHECK_EQUAL(exact_pow(Number(2.0), Number(1, 2)).value(), Number(std::sqrt(2.0)));
    LODE_CHECK_THROWS(exact_pow(Number(0), Number(-1)), std::domain_error);

    // Numbers order by value, with a rational before a real of equal value
    LODE_CHECK(compare(Number(1, 3), Number(1, 2)) < 0);
    LODE_CHECK(compare(Number(-1), Number(0.5)) < 0);
    LODE_CHECK(compare(Number(2), Number(2.0)) < 0);
    LODE_CHECK(compare(Number(2, 4), Number(1, 2)) == 0);

    // Text forms
    LODE_CHECK_EQUAL(Number(-3, 4).to_string(), std::string("-3/4"));
    LODE_CHECK_EQUAL(Number(5).to_string(), std::string("5"));
    LODE_CHECK_EQUAL(Number(2.0).to_string(), std::string("2.0"));
    LODE_CHECK_EQUAL(Number(0.1).to_string(), std::string("0.1"));
    LODE_CHECK_EQUAL(Number(1e300 * 1e10).to_string(), std::string("inf"));

    // Equal numbers hash equally, including the two signed zeros
    LODE_CHECK_EQUAL(Number(-0.0).hash(), Number(0.0).hash());
    LODE_CHECK_EQUAL(Number(2, 4).hash(), Number(1, 2).hash());

    return lode::test::report();
}
