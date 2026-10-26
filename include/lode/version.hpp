/*--------------------------------------------------------------------------------------------------
 * File: version.hpp
 *
 * Description: Version number of the LODE library.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_VERSION_HPP
#define LODE_VERSION_HPP

#include <string_view>

namespace lode
{
    inline constexpr int              VERSION_MAJOR = 0;
    inline constexpr int              VERSION_MINOR = 1;
    inline constexpr int              VERSION_PATCH = 0;
    inline constexpr std::string_view VERSION       = "0.1.0";
}
#endif
