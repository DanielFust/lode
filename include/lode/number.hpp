/*--------------------------------------------------------------------------------------------------
 * File: number.hpp
 *
 * Description: Numbers in LODE expressions: exact rationals with 64-bit parts, and floating-point
 *              reals.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_NUMBER_HPP
#define LODE_NUMBER_HPP

#include <charconv>
#include <cmath>
#include <compare>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace lode
{
    namespace detail
    {
        // Wide integer for exact intermediate results of 64-bit rational arithmetic (GCC, Clang)
        using Int128 = __int128;

        // Mixes a value's hash into a running seed
        inline void hash_combine(std::size_t& seed, std::size_t value)
        {
            constexpr std::size_t GOLDEN = static_cast<std::size_t>(0x9e3779b97f4a7c15ULL);
            seed ^= value + GOLDEN + (seed << 6) + (seed >> 2);
        }

        // Whether a wide integer fits in 64 bits
        inline bool fits_int64(Int128 value)
        {
            return value >= std::numeric_limits<std::int64_t>::min()
                && value <= std::numeric_limits<std::int64_t>::max();
        }


       /*-------------------------------------------------------------------------------------------
        * Int128 gcd(Int128 a, Int128 b)
        *
        * Greatest common divisor of two non-negative wide integers.
        *-----------------------------------------------------------------------------------------*/
        inline Int128 gcd(Int128 a, Int128 b)
        {
            while (b != 0)
            {
                Int128 remainder = a % b;
                a = b;
                b = remainder;
            }
            return a;
        }


       /*-------------------------------------------------------------------------------------------
        * std::optional<std::int64_t> integer_power(std::int64_t base, std::uint64_t exponent)
        *
        * Raises an integer to a non-negative integer power exactly.
        *
        * Returns:
        *     The power, or nothing if it does not fit in 64 bits
        *-----------------------------------------------------------------------------------------*/
        inline std::optional<std::int64_t> integer_power(std::int64_t base, std::uint64_t exponent)
        {
            // 0, 1 and -1 never overflow, whatever the exponent
            if (base == 0) return exponent == 0 ? 1 : 0;
            if (base == 1) return 1;
            if (base == -1) return exponent % 2 == 0 ? 1 : -1;

            // Any other base overflows 64 bits beyond exponent 63
            if (exponent > 63) return std::nullopt;
            Int128 result = 1;
            for (std::uint64_t i = 0; i < exponent; ++i)
            {
                result *= base;
                if (!fits_int64(result)) return std::nullopt;
            }
            return static_cast<std::int64_t>(result);
        }


       /*-------------------------------------------------------------------------------------------
        * std::optional<std::int64_t> integer_root(std::int64_t value, std::uint64_t degree)
        *
        * Exact root of a non-negative integer.
        *
        * Parameters:
        *     value     Non-negative integer
        *     degree    Degree of the root (2 for a square root), at least 2
        *
        * Returns:
        *     The integer r with r^degree = value, or nothing if there is none
        *-----------------------------------------------------------------------------------------*/
        inline std::optional<std::int64_t> integer_root(std::int64_t value, std::uint64_t degree)
        {
            if (value == 0 || value == 1) return value;

            // Estimate in floating point, then check the estimate and its neighbours exactly
            double estimate = std::round(std::pow(static_cast<double>(value),
                                                  1.0 / static_cast<double>(degree)));
            std::int64_t guess = static_cast<std::int64_t>(estimate);
            for (std::int64_t candidate = guess - 1; candidate <= guess + 1; ++candidate)
            {
                if (candidate < 2) continue;
                std::optional<std::int64_t> power = integer_power(candidate, degree);
                if (power && *power == value) return candidate;
            }
            return std::nullopt;
        }
    }


   /*-----------------------------------------------------------------------------------------------
    * class Number
    *
    * A number in an expression: either an exact rational with 64-bit numerator and denominator,
    * or a floating-point real.
    *
    * Arithmetic on rationals is exact while the result fits in 64 bits, and gives a real when it
    * would not. Any operation involving a real gives a real. A rational and a real are never
    * equal, even with the same value (2 and 2.0 are different numbers), so exact and inexact
    * constants stay apart. NaN is not a valid number.
    *---------------------------------------------------------------------------------------------*/
    class Number
    {
      private:
        std::int64_t numerator_   = 0;     // A rational's value is numerator_ / denominator_,
        std::int64_t denominator_ = 1;     // in lowest terms with a positive denominator
        double       real_        = 0.0;   // A real's value
        bool         is_real_     = false; // Whether this is a real rather than a rational

      public:
        /*------------------------------------- Constructors -------------------------------------*/

        // Zero (exact)
        Number() = default;

        // An exact integer. Implicit, so that integers mix freely with numbers; a value outside
        // the 64-bit range becomes a real.
        template<std::integral I>
            requires (!std::same_as<I, bool>)
        Number(I value);

        // A real; throws std::domain_error for NaN. Implicit for the same reason.
        template<std::floating_point F>
        Number(F value);

        // The exact rational numerator/denominator; throws std::domain_error if the denominator
        // is zero
        Number(std::int64_t numerator, std::int64_t denominator);

        /*--------------------------------------- Queries ----------------------------------------*/

        bool is_rational() const {return !this->is_real_;}
        bool is_real() const {return this->is_real_;}
        bool is_integer() const {return !this->is_real_ && this->denominator_ == 1;}
        bool is_zero() const {return this->is_real_ ? this->real_ == 0.0 : this->numerator_ == 0;}
        bool is_negative() const {return this->is_real_ ? this->real_ < 0.0 : this->numerator_ < 0;}

        // Whether this is exactly 1 (a real is never exactly 1)
        bool is_one() const
        {return !this->is_real_ && this->numerator_ == 1 && this->denominator_ == 1;}

        // Numerator and denominator of a rational; throw std::logic_error for a real
        std::int64_t numerator() const;
        std::int64_t denominator() const;

        // Value as a double
        double to_double() const;

        // Hash; equal numbers have equal hashes
        std::size_t hash() const;

        // Text form: "3", "-3/4", or a real such as "2.0" or "0.1"
        std::string to_string() const;

        /*-------------------------------------- Arithmetic --------------------------------------*/

        Number operator-() const;

        friend Number operator+(Number const& a, Number const& b);
        friend Number operator-(Number const& a, Number const& b);
        friend Number operator*(Number const& a, Number const& b);
        friend Number operator/(Number const& a, Number const& b);

        Number& operator+=(Number const& b) {return *this = *this + b;}
        Number& operator-=(Number const& b) {return *this = *this - b;}
        Number& operator*=(Number const& b) {return *this = *this * b;}
        Number& operator/=(Number const& b) {return *this = *this / b;}

        /*-------------------------------------- Comparison --------------------------------------*/

        // Same kind (rational or real) and same value
        friend bool operator==(Number const& a, Number const& b);

        // Order by value; a rational sorts before a real of equal value
        friend std::strong_ordering compare(Number const& a, Number const& b);

      private:
        // Exact rational from wide parts, or a real if it does not fit in 64 bits
        static Number from_ratio(detail::Int128 numerator, detail::Int128 denominator);
    };

    // Exact power base^exponent; see the definition for when there is no result
    std::optional<Number> exact_pow(Number const& base, Number const& exponent);

    // Writes the number's text form
    std::ostream& operator<<(std::ostream& out, Number const& number);


   /*-----------------------------------------------------------------------------------------------
    * Number::Number(I value)
    *
    * An exact integer, or a real if the value is outside the 64-bit range.
    *
    * Template Parameters:
    *     I         Integer type (not bool)
    *---------------------------------------------------------------------------------------------*/
    template<std::integral I>
        requires (!std::same_as<I, bool>)
    Number::Number(I value)
    {
        if (std::in_range<std::int64_t>(value))
            this->numerator_ = static_cast<std::int64_t>(value);
        else
            *this = Number(static_cast<double>(value));
    }


   /*-----------------------------------------------------------------------------------------------
    * Number::Number(F value)
    *
    * A real. Negative zero becomes zero, so that equal values hash equally.
    *
    * Template Parameters:
    *     F         Floating-point type
    *---------------------------------------------------------------------------------------------*/
    template<std::floating_point F>
    Number::Number(F value) : real_(static_cast<double>(value)), is_real_(true)
    {
        if (std::isnan(this->real_)) throw std::domain_error("Number: NaN is not a valid number");
        if (this->real_ == 0.0) this->real_ = 0.0;
    }


   /*-----------------------------------------------------------------------------------------------
    * Number::Number(std::int64_t numerator, std::int64_t denominator)
    *
    * The exact rational numerator/denominator, reduced to lowest terms.
    *---------------------------------------------------------------------------------------------*/
    inline Number::Number(std::int64_t numerator, std::int64_t denominator)
    {
        if (denominator == 0)
        {
            throw std::domain_error("Number: zero denominator in " + std::to_string(numerator)
                                    + "/0");
        }
        *this = Number::from_ratio(numerator, denominator);
    }


   /*-----------------------------------------------------------------------------------------------
    * Number Number::from_ratio(Int128 numerator, Int128 denominator)
    *
    * Reduces a ratio of wide integers to lowest terms with a positive denominator.
    *
    * Parameters:
    *     numerator      Numerator
    *     denominator    Denominator (nonzero)
    *
    * Returns:
    *     The exact rational if both parts fit in 64 bits, otherwise the nearest real
    *---------------------------------------------------------------------------------------------*/
    inline Number Number::from_ratio(detail::Int128 numerator, detail::Int128 denominator)
    {
        if (denominator < 0)
        {
            numerator   = -numerator;
            denominator = -denominator;
        }
        detail::Int128 divisor = detail::gcd(numerator < 0 ? -numerator : numerator, denominator);
        numerator   /= divisor;
        denominator /= divisor;

        if (!detail::fits_int64(numerator) || !detail::fits_int64(denominator))
            return Number(static_cast<double>(numerator) / static_cast<double>(denominator));

        Number result;
        result.numerator_   = static_cast<std::int64_t>(numerator);
        result.denominator_ = static_cast<std::int64_t>(denominator);
        return result;
    }


   /*-----------------------------------------------------------------------------------------------
    * std::int64_t Number::numerator() const
    *
    * Numerator of a rational, in lowest terms; throws std::logic_error for a real.
    *---------------------------------------------------------------------------------------------*/
    inline std::int64_t Number::numerator() const
    {
        if (this->is_real_)
            throw std::logic_error("Number::numerator: " + this->to_string() + " is not rational");
        return this->numerator_;
    }


   /*-----------------------------------------------------------------------------------------------
    * std::int64_t Number::denominator() const
    *
    * Denominator of a rational, positive and in lowest terms; throws std::logic_error for a real.
    *---------------------------------------------------------------------------------------------*/
    inline std::int64_t Number::denominator() const
    {
        if (this->is_real_)
            throw std::logic_error("Number::denominator: " + this->to_string() + " is not rational");
        return this->denominator_;
    }


   /*-----------------------------------------------------------------------------------------------
    * double Number::to_double() const
    *
    * Value as a double, rounded for a rational that a double cannot represent exactly.
    *---------------------------------------------------------------------------------------------*/
    inline double Number::to_double() const
    {
        if (this->is_real_) return this->real_;
        return static_cast<double>(this->numerator_) / static_cast<double>(this->denominator_);
    }


   /*-----------------------------------------------------------------------------------------------
    * std::size_t Number::hash() const
    *
    * Hash of the kind and value.
    *---------------------------------------------------------------------------------------------*/
    inline std::size_t Number::hash() const
    {
        std::size_t seed = std::hash<bool>{}(this->is_real_);
        if (this->is_real_)
            detail::hash_combine(seed, std::hash<double>{}(this->real_));
        else
        {
            detail::hash_combine(seed, std::hash<std::int64_t>{}(this->numerator_));
            detail::hash_combine(seed, std::hash<std::int64_t>{}(this->denominator_));
        }
        return seed;
    }


   /*-----------------------------------------------------------------------------------------------
    * std::string Number::to_string() const
    *
    * Text form. A rational prints as "3" or "-3/4". A real prints as the shortest text that
    * reads back to the same double, with a decimal point added if it would otherwise look like
    * an integer ("2.0").
    *---------------------------------------------------------------------------------------------*/
    inline std::string Number::to_string() const
    {
        if (!this->is_real_)
        {
            std::string text = std::to_string(this->numerator_);
            if (this->denominator_ != 1) text += "/" + std::to_string(this->denominator_);
            return text;
        }
        if (std::isinf(this->real_)) return this->real_ > 0.0 ? "inf" : "-inf";

        char buffer[32];
        std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), this->real_);
        std::string text(buffer, result.ptr);
        if (text.find_first_of(".e") == std::string::npos) text += ".0";
        return text;
    }


   /*-----------------------------------------------------------------------------------------------
    * Number Number::operator-() const
    *
    * Negation.
    *---------------------------------------------------------------------------------------------*/
    inline Number Number::operator-() const
    {
        if (this->is_real_) return Number(-this->real_);
        return Number::from_ratio(-static_cast<detail::Int128>(this->numerator_), this->denominator_);
    }


   /*-----------------------------------------------------------------------------------------------
    * Number operator+(Number const& a, Number const& b)
    *
    * Sum; exact for rationals unless the result overflows.
    *---------------------------------------------------------------------------------------------*/
    inline Number operator+(Number const& a, Number const& b)
    {
        if (a.is_real_ || b.is_real_) return Number(a.to_double() + b.to_double());
        using detail::Int128;
        return Number::from_ratio(static_cast<Int128>(a.numerator_) * b.denominator_
                                      + static_cast<Int128>(b.numerator_) * a.denominator_,
                                  static_cast<Int128>(a.denominator_) * b.denominator_);
    }


   /*-----------------------------------------------------------------------------------------------
    * Number operator-(Number const& a, Number const& b)
    *
    * Difference; exact for rationals unless the result overflows.
    *---------------------------------------------------------------------------------------------*/
    inline Number operator-(Number const& a, Number const& b)
    {
        if (a.is_real_ || b.is_real_) return Number(a.to_double() - b.to_double());
        using detail::Int128;
        return Number::from_ratio(static_cast<Int128>(a.numerator_) * b.denominator_
                                      - static_cast<Int128>(b.numerator_) * a.denominator_,
                                  static_cast<Int128>(a.denominator_) * b.denominator_);
    }


   /*-----------------------------------------------------------------------------------------------
    * Number operator*(Number const& a, Number const& b)
    *
    * Product; exact for rationals unless the result overflows.
    *---------------------------------------------------------------------------------------------*/
    inline Number operator*(Number const& a, Number const& b)
    {
        if (a.is_real_ || b.is_real_) return Number(a.to_double() * b.to_double());
        using detail::Int128;
        return Number::from_ratio(static_cast<Int128>(a.numerator_) * b.numerator_,
                                  static_cast<Int128>(a.denominator_) * b.denominator_);
    }


   /*-----------------------------------------------------------------------------------------------
    * Number operator/(Number const& a, Number const& b)
    *
    * Quotient; exact for rationals unless the result overflows. Throws std::domain_error if b is
    * zero, for reals as well as rationals.
    *---------------------------------------------------------------------------------------------*/
    inline Number operator/(Number const& a, Number const& b)
    {
        if (b.is_zero())
        {
            throw std::domain_error("Number: division by zero (" + a.to_string() + "/"
                                    + b.to_string() + ")");
        }
        if (a.is_real_ || b.is_real_) return Number(a.to_double() / b.to_double());
        using detail::Int128;
        return Number::from_ratio(static_cast<Int128>(a.numerator_) * b.denominator_,
                                  static_cast<Int128>(a.denominator_) * b.numerator_);
    }


   /*-----------------------------------------------------------------------------------------------
    * bool operator==(Number const& a, Number const& b)
    *
    * Whether two numbers are the same kind (rational or real) with the same value.
    *---------------------------------------------------------------------------------------------*/
    inline bool operator==(Number const& a, Number const& b)
    {
        if (a.is_real_ != b.is_real_) return false;
        if (a.is_real_) return a.real_ == b.real_;
        return a.numerator_ == b.numerator_ && a.denominator_ == b.denominator_;
    }


   /*-----------------------------------------------------------------------------------------------
    * std::strong_ordering compare(Number const& a, Number const& b)
    *
    * Orders numbers by value, comparing rationals exactly. Of a rational and a real with equal
    * values, the rational sorts first, so the order is consistent with operator==.
    *---------------------------------------------------------------------------------------------*/
    inline std::strong_ordering compare(Number const& a, Number const& b)
    {
        if (!a.is_real_ && !b.is_real_)
        {
            using detail::Int128;
            Int128 lhs = static_cast<Int128>(a.numerator_) * b.denominator_;
            Int128 rhs = static_cast<Int128>(b.numerator_) * a.denominator_;
            if (lhs < rhs) return std::strong_ordering::less;
            if (lhs > rhs) return std::strong_ordering::greater;
            return std::strong_ordering::equal;
        }

        double x = a.to_double();
        double y = b.to_double();
        if (x < y) return std::strong_ordering::less;
        if (x > y) return std::strong_ordering::greater;
        if (a.is_real_ != b.is_real_)
            return a.is_real_ ? std::strong_ordering::greater : std::strong_ordering::less;
        return std::strong_ordering::equal;
    }


   /*-----------------------------------------------------------------------------------------------
    * std::optional<Number> exact_pow(Number const& base, Number const& exponent)
    *
    * Raises a number to a power, exactly when both are rational.
    *
    * A rational power p/q of a rational base is exact when the base is a perfect q-th power
    * (4^(1/2) = 2, (-8)^(1/3) = -2); otherwise there is no result, so that the caller can keep
    * the power symbolic (2^(1/2)). An odd root of a negative base is the real root. If either
    * number is real, the power is computed in floating point. An exact result that does not fit
    * in 64 bits becomes a real.
    *
    * Parameters:
    *     base        Base
    *     exponent    Exponent
    *
    * Returns:
    *     The power, or nothing if it is irrational or not real (an even root of a negative number)
    *
    * Throws std::domain_error for zero raised to a negative power.
    *---------------------------------------------------------------------------------------------*/
    inline std::optional<Number> exact_pow(Number const& base, Number const& exponent)
    {
        if (base.is_zero() && exponent.is_negative())
            throw std::domain_error("Number: division by zero (0^" + exponent.to_string() + ")");

        // Reals: floating point, unless the result would not be real
        if (base.is_real() || exponent.is_real())
        {
            double b = base.to_double();
            double x = exponent.to_double();
            if (b < 0.0 && x != std::floor(x)) return std::nullopt;
            return Number(std::pow(b, x));
        }

        if (exponent.is_zero()) return Number(1);
        if (base.is_zero()) return Number(0);
        if (exponent.is_negative()) return exact_pow(Number(1) / base, -exponent);

        // Exponent p/q > 0: take the q-th root exactly, then raise it to the power p
        std::int64_t p    = exponent.numerator();
        std::int64_t q    = exponent.denominator();
        Number       root = base;
        if (q != 1)
        {
            bool negative = base.is_negative();
            if (negative && q % 2 == 0) return std::nullopt;
            if (base.numerator() == std::numeric_limits<std::int64_t>::min()) return std::nullopt;

            std::int64_t magnitude = negative ? -base.numerator() : base.numerator();
            std::optional<std::int64_t> root_numerator =
                detail::integer_root(magnitude, static_cast<std::uint64_t>(q));
            std::optional<std::int64_t> root_denominator =
                detail::integer_root(base.denominator(), static_cast<std::uint64_t>(q));
            if (!root_numerator || !root_denominator) return std::nullopt;
            root = Number(negative ? -*root_numerator : *root_numerator, *root_denominator);
        }

        std::optional<std::int64_t> numerator =
            detail::integer_power(root.numerator(), static_cast<std::uint64_t>(p));
        std::optional<std::int64_t> denominator =
            detail::integer_power(root.denominator(), static_cast<std::uint64_t>(p));
        if (numerator && denominator) return Number(*numerator, *denominator);
        return Number(std::pow(root.to_double(), static_cast<double>(p)));
    }


    // Writes the number's text form
    inline std::ostream& operator<<(std::ostream& out, Number const& number)
    {return out << number.to_string();}
}
#endif
