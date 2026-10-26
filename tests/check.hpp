/*--------------------------------------------------------------------------------------------------
 * File: check.hpp
 *
 * Description: A minimal test harness. LODE_CHECK records a failed condition with its location
 *              and keeps going; lode::test::report() prints a summary and gives the exit code.
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
    }


   /*-----------------------------------------------------------------------------------------------
    * void check(bool passed, char const* expression, std::source_location where)
    *
    * Records the result of one check and prints the failing expression and its location.
    *
    * Parameters:
    *     passed        Whether the checked condition held
    *     expression    Text of the condition, for the failure message
    *     where         Location of the check in the test source
    *---------------------------------------------------------------------------------------------*/
    inline void check(bool passed, char const* expression,
                      std::source_location where = std::source_location::current())
    {
        ++detail::checks;
        if (passed) return;
        ++detail::failures;
        std::cerr << where.file_name() << ":" << where.line() << ": check failed: "
                  << expression << "\n";
    }


   /*-----------------------------------------------------------------------------------------------
    * int report()
    *
    * Prints how many checks ran and failed.
    *
    * Returns:
    *     0 if every check passed, 1 otherwise (for use as the test's exit code)
    *---------------------------------------------------------------------------------------------*/
    inline int report()
    {
        std::cout << detail::checks - detail::failures << " of " << detail::checks
                  << " checks passed\n";
        return detail::failures == 0 ? 0 : 1;
    }
}

// Checks a condition, recording the source text and location if it fails.
#define LODE_CHECK(condition) ::lode::test::check(static_cast<bool>(condition), #condition)

#endif
