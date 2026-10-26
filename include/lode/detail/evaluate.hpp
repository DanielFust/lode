/*--------------------------------------------------------------------------------------------------
 * File: evaluate.hpp
 *
 * Description: The reference evaluator, which computes an expression's value by walking it.
 *              Included by expr.hpp; not meant to be included on its own.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_DETAIL_EVALUATE_HPP
#define LODE_DETAIL_EVALUATE_HPP

#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <unordered_map>

namespace lode
{
    namespace detail
    {
       /*-------------------------------------------------------------------------------------------
        * T power_by_squaring(T base, std::int64_t exponent)
        *
        * An integer power by repeated squaring: exact steps, no logarithms, so it is accurate
        * for negative bases and suits complex numbers.
        *
        * Template Parameters:
        *     T           Number type
        *
        * Parameters:
        *     base        Base
        *     exponent    Integer exponent; a negative one gives the reciprocal
        *-----------------------------------------------------------------------------------------*/
        template<class T>
        T power_by_squaring(T base, std::int64_t exponent)
        {
            // Magnitude of the exponent, valid even for the most negative 64-bit integer
            std::uint64_t remaining = exponent < 0 ? 0 - static_cast<std::uint64_t>(exponent)
                                                   : static_cast<std::uint64_t>(exponent);
            T result = static_cast<T>(1);
            while (remaining != 0)
            {
                if (remaining & 1) result *= base;
                remaining >>= 1;
                if (remaining != 0) base *= base;
            }
            return exponent < 0 ? static_cast<T>(1) / result : result;
        }


       /*-------------------------------------------------------------------------------------------
        * class Evaluator<T>
        *
        * Evaluates expressions for given values of their variables. Each node's value is
        * remembered, so a shared subexpression is evaluated once.
        *
        * Template Parameters:
        *     T         Number type, such as double or std::complex<double>
        *-----------------------------------------------------------------------------------------*/
        template<class T>
        class Evaluator
        {
          private:
            std::unordered_map<std::uint64_t, T> values_;  // Values by variable identifier
            std::unordered_map<Node const*, T>   results_; // Values found so far

          public:
            // Throws std::invalid_argument if a variable is given twice
            explicit Evaluator(Values<T> const& values);

            // Value of an expression
            T operator()(Expr const& expr);

          private:
            // Value of an expression not yet evaluated
            T compute(Expr const& expr);

            // Value of base^exponent
            T power(Expr const& base, Expr const& exponent);
        };


       /*-------------------------------------------------------------------------------------------
        * Evaluator<T>::Evaluator(Values<T> const& values)
        *
        * Indexes the values by variable; throws std::invalid_argument if a variable is given
        * twice.
        *-----------------------------------------------------------------------------------------*/
        template<class T>
        Evaluator<T>::Evaluator(Values<T> const& values)
        {
            for (auto const& [variable, value] : values)
            {
                if (!this->values_.try_emplace(variable.id(), value).second)
                {
                    throw std::invalid_argument("evaluate: '" + variable.name()
                                                + "' is given more than once");
                }
            }
        }


       /*-------------------------------------------------------------------------------------------
        * T Evaluator<T>::operator()(Expr const& expr)
        *
        * Value of an expression, computed once per node.
        *-----------------------------------------------------------------------------------------*/
        template<class T>
        T Evaluator<T>::operator()(Expr const& expr)
        {
            auto it = this->results_.find(&expr.node());
            if (it != this->results_.end()) return it->second;
            T value = this->compute(expr);
            this->results_.emplace(&expr.node(), value);
            return value;
        }


       /*-------------------------------------------------------------------------------------------
        * T Evaluator<T>::compute(Expr const& expr)
        *
        * Value of an expression from the values of its children. Throws std::invalid_argument
        * for a variable with no value.
        *-----------------------------------------------------------------------------------------*/
        template<class T>
        T Evaluator<T>::compute(Expr const& expr)
        {
            // The standard functions, found for T by argument-dependent lookup if it is a class
            using std::cos;
            using std::exp;
            using std::log;
            using std::sin;

            switch (expr.kind())
            {
                case Kind::Number:
                    return static_cast<T>(expr.number().to_double());

                case Kind::Symbol:
                {
                    SymbolData const& symbol = expr.node().symbol();
                    auto it = this->values_.find(symbol.id);
                    if (it == this->values_.end())
                    {
                        throw std::invalid_argument("evaluate: no value given for '" + symbol.name
                                                    + "'");
                    }
                    return it->second;
                }

                case Kind::Add:
                {
                    AddData const& sum = expr.node().add();
                    T value = static_cast<T>(sum.constant.to_double());
                    for (Term const& term : sum.terms)
                        value += static_cast<T>(term.coefficient.to_double()) * (*this)(term.expr);
                    return value;
                }

                case Kind::Mul:
                {
                    MulData const& product = expr.node().mul();
                    T value = static_cast<T>(product.coefficient.to_double());
                    for (Factor const& factor : product.factors)
                        value *= this->power(factor.base, factor.exponent);
                    return value;
                }

                case Kind::Pow:
                    return this->power(expr.node().pow().base, expr.node().pow().exponent);

                case Kind::Call:
                {
                    CallData const& call = expr.node().call();
                    T argument = (*this)(call.argument);
                    switch (call.function)
                    {
                        case Elementary::Exp: return exp(argument);
                        case Elementary::Log: return log(argument);
                        case Elementary::Sin: return sin(argument);
                        case Elementary::Cos: return cos(argument);
                    }
                    break;
                }
            }
            return static_cast<T>(0);
        }


       /*-------------------------------------------------------------------------------------------
        * T Evaluator<T>::power(Expr const& base, Expr const& exponent)
        *
        * Value of base^exponent: by repeated squaring for an integer exponent, otherwise with pow.
        *-----------------------------------------------------------------------------------------*/
        template<class T>
        T Evaluator<T>::power(Expr const& base, Expr const& exponent)
        {
            using std::pow;

            T value = (*this)(base);
            if (exponent.is_number() && exponent.number().is_integer())
                return power_by_squaring(value, exponent.number().numerator());
            return pow(value, (*this)(exponent));
        }
    }


   /*-----------------------------------------------------------------------------------------------
    * T evaluate(Expr const& expr, Values<T> const& values)
    *
    * Numeric value of an expression for given values of its variables.
    *
    * Template Parameters:
    *     T         Number type (double unless given)
    *
    * Parameters:
    *     expr      Expression to evaluate
    *     values    Value of each variable in the expression; others are ignored
    *
    * Returns:
    *     The value. Throws std::invalid_argument if a variable has no value or is given twice.
    *---------------------------------------------------------------------------------------------*/
    template<class T>
    T evaluate(Expr const& expr, Values<T> const& values)
    {
        detail::Evaluator<T> evaluator(values);
        return evaluator(expr);
    }
}
#endif
