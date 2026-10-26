/*--------------------------------------------------------------------------------------------------
 * File: check.hpp
 *
 * Description: A minimal test harness. The LODE_CHECK macros record a failed check with its
 *              location and keep going; lode::test::report() prints a summary and gives the exit
 *              code.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_TESTS_CHECK_HPP
#define LODE_TESTS_CHECK_HPP

#include <iostream>
#include <source_location>

namespace lode::test
{
    namespace detail
    {
        inline int checks   = 0;
        inline int failures = 0;

        // Counts a check, and prints the location of a failed one
        inline bool record(bool passed, std::source_location const& where)
        {
            ++checks;
            if (passed) return true;
            ++failures;
            std::cerr << where.file_name() << ":" << where.line() << ": check failed: ";
            return false;
        }
    }


   /*-----------------------------------------------------------------------------------------------
    * void check(bool passed, char const* expression, std::source_location where)
    *
    * Records the result of one check, printing the failed expression and its location.
    *
    * Parameters:
    *     passed        Whether the condition held
    *     expression    Text of the condition
    *     where         Location of the check in the test
    *---------------------------------------------------------------------------------------------*/
    inline void check(bool passed, char const* expression,
                      std::source_location where = std::source_location::current())
    {
        if (!detail::record(passed, where)) std::cerr << expression << "\n";
    }


   /*-----------------------------------------------------------------------------------------------
    * void check_equal(A const& actual, B const& expected, char const* actual_text,
    *                  char const* expected_text, std::source_location where)
    *
    * Records whether two values are equal, printing both on failure.
    *
    * Template Parameters:
    *     A, B             Types comparable with == and printable with <<
    *
    * Parameters:
    *     actual           Value computed by the test
    *     expected         Value it should equal
    *     actual_text      Source text of actual
    *     expected_text    Source text of expected
    *     where            Location of the check in the test
    *---------------------------------------------------------------------------------------------*/
    template<class A, class B>
    void check_equal(A const& actual, B const& expected, char const* actual_text,
                     char const* expected_text,
                     std::source_location where = std::source_location::current())
    {
        if (!detail::record(actual == expected, where))
        {
            std::cerr << actual_text << " == " << expected_text << "\n"
                      << "    actual:   " << actual << "\n"
                      << "    expected: " << expected << "\n";
        }
    }


   /*-----------------------------------------------------------------------------------------------
    * int report()
    *
    * Prints how many checks ran and passed.
    *
    * Returns:
    *     0 if every check passed, 1 otherwise, for use as the test's exit code
    *---------------------------------------------------------------------------------------------*/
    inline int report()
    {
        std::cout << detail::checks - detail::failures << " of " << detail::checks
                  << " checks passed\n";
        return detail::failures == 0 ? 0 : 1;
    }
}

// Checks that a condition holds
#define LODE_CHECK(condition) ::lode::test::check(static_cast<bool>(condition), #condition)

// Checks that two values are equal, printing both if not
#define LODE_CHECK_EQUAL(actual, expected) \
    ::lode::test::check_equal((actual), (expected), #actual, #expected)

// Checks that evaluating an expression throws the given exception type
#define LODE_CHECK_THROWS(expression, exception)                                                   \
    ::lode::test::check(                                                                           \
        [&]()                                                                                      \
        {                                                                                          \
            try                                                                                    \
            {                                                                                      \
                static_cast<void>(expression);                                                     \
            }                                                                                      \
            catch (exception const&)                                                               \
            {                                                                                      \
                return true;                                                                       \
            }                                                                                      \
            catch (...)                                                                            \
            {                                                                                      \
            }                                                                                      \
            return false;                                                                          \
        }(),                                                                                       \
        #expression " throws " #exception)

#endif
