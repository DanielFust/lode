/*--------------------------------------------------------------------------------------------------
 * File: version_test.cpp
 *
 * Description: Checks that the version constants agree with each other.
 *------------------------------------------------------------------------------------------------*/
#include <string>

#include "check.hpp"
#include "lode/lode.hpp"

int main()
{
    std::string expected = std::to_string(lode::VERSION_MAJOR) + "."
                         + std::to_string(lode::VERSION_MINOR) + "."
                         + std::to_string(lode::VERSION_PATCH);
    LODE_CHECK(lode::VERSION == expected);
    return lode::test::report();
}
