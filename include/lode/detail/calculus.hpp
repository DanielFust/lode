/*--------------------------------------------------------------------------------------------------
 * File: calculus.hpp
 *
 * Description: Differentiation, substitution, and listing the variables of an expression.
 *              Included by expr.hpp; not meant to be included on its own.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_DETAIL_CALCULUS_HPP
#define LODE_DETAIL_CALCULUS_HPP

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace lode
{
    namespace detail
    {
        // Whether an expression is the number zero
        inline bool is_zero_expr(Expr const& expr)
        {return expr.is_number() && expr.number().is_zero();}

        // Whether two expressions share the same node (a cheap test for "unchanged")
        inline bool same_node(Expr const& a, Expr const& b) {return &a.node() == &b.node();}


       /*-------------------------------------------------------------------------------------------
        * std::vector<Expr> children(Expr const& expr)
        *
        * The subexpressions an expression is built from: a sum's terms, a product's bases and
        * exponents, a power's base and exponent, or a call's argument.
        *-----------------------------------------------------------------------------------------*/
        inline std::vector<Expr> children(Expr const& expr)
        {
            std::vector<Expr> parts;
            switch (expr.kind())
            {
                case Kind::Number:
                case Kind::Symbol:
                    break;
                case Kind::Add:
                    for (Term const& term : expr.node().add().terms) parts.push_back(term.expr);
                    break;
                case Kind::Mul:
                    for (Factor const& factor : expr.node().mul().factors)
                    {
                        parts.push_back(factor.base);
                        parts.push_back(factor.exponent);
                    }
                    break;
                case Kind::Pow:
                    parts.push_back(expr.node().pow().base);
                    parts.push_back(expr.node().pow().exponent);
                    break;
                case Kind::Call:
                    parts.push_back(expr.node().call().argument);
                    break;
            }
            return parts;
        }


       /*-------------------------------------------------------------------------------------------
        * class Differentiator
        *
        * Differentiates expressions with respect to one variable. Each node's derivative is
        * remembered, so a subexpression that appears in several places is differentiated once
        * and its derivative is shared in turn. The remembered nodes all belong to the expression
        * being differentiated, which the caller keeps alive.
        *-----------------------------------------------------------------------------------------*/
        class Differentiator
        {
          private:
            std::uint64_t                         variable_;    // Identifier of the variable
            std::unordered_map<Node const*, Expr> derivatives_; // Derivatives found so far

          public:
            explicit Differentiator(Symbol const& variable) : variable_(variable.id()) {}

            // Derivative of an expression
            Expr operator()(Expr const& expr);

          private:
            // Derivative of an expression not yet differentiated
            Expr compute(Expr const& expr);

            // Derivative of base^exponent
            Expr power_derivative(Expr const& base, Expr const& exponent);
        };


       /*-------------------------------------------------------------------------------------------
        * Expr Differentiator::operator()(Expr const& expr)
        *
        * Derivative of an expression, computed once per node.
        *-----------------------------------------------------------------------------------------*/
        inline Expr Differentiator::operator()(Expr const& expr)
        {
            auto it = this->derivatives_.find(&expr.node());
            if (it != this->derivatives_.end()) return it->second;
            Expr derivative = this->compute(expr);
            this->derivatives_.emplace(&expr.node(), derivative);
            return derivative;
        }


       /*-------------------------------------------------------------------------------------------
        * Expr Differentiator::compute(Expr const& expr)
        *
        * Derivative by the sum, product and chain rules. The product rule for a product of n
        * factors gives n terms, each with one factor differentiated; factors that do not depend
        * on the variable contribute no term.
        *-----------------------------------------------------------------------------------------*/
        inline Expr Differentiator::compute(Expr const& expr)
        {
            switch (expr.kind())
            {
                case Kind::Number:
                    return zero();

                case Kind::Symbol:
                    return expr.node().symbol().id == this->variable_ ? one() : zero();

                case Kind::Add:
                {
                    std::vector<Expr> terms;
                    for (Term const& term : expr.node().add().terms)
                    {
                        Expr derivative = (*this)(term.expr);
                        if (!is_zero_expr(derivative))
                            terms.push_back(make_mul({Expr(term.coefficient), derivative}));
                    }
                    return make_add(terms);
                }

                case Kind::Mul:
                {
                    MulData const& product = expr.node().mul();
                    std::vector<Expr> terms;
                    for (std::size_t i = 0; i < product.factors.size(); ++i)
                    {
                        Factor const& factor = product.factors[i];
                        Expr derivative = this->power_derivative(factor.base, factor.exponent);
                        if (is_zero_expr(derivative)) continue;

                        std::vector<Expr> operands{Expr(product.coefficient), derivative};
                        for (std::size_t j = 0; j < product.factors.size(); ++j)
                        {
                            if (j == i) continue;
                            Factor const& other = product.factors[j];
                            operands.push_back(make_pow(other.base, other.exponent));
                        }
                        terms.push_back(make_mul(operands));
                    }
                    return make_add(terms);
                }

                case Kind::Pow:
                    return this->power_derivative(expr.node().pow().base, expr.node().pow().exponent);

                case Kind::Call:
                {
                    CallData const& call = expr.node().call();
                    Expr inner = (*this)(call.argument);
                    if (is_zero_expr(inner)) return zero();

                    // Derivative of the function itself, at the argument
                    Expr outer;
                    switch (call.function)
                    {
                        case Elementary::Exp:
                            outer = expr;
                            break;
                        case Elementary::Log:
                            outer = make_pow(call.argument, Expr(-1));
                            break;
                        case Elementary::Sin:
                            outer = make_call(Elementary::Cos, call.argument);
                            break;
                        case Elementary::Cos:
                            outer = make_mul({Expr(-1), make_call(Elementary::Sin, call.argument)});
                            break;
                    }
                    return make_mul({outer, inner});
                }
            }
            return zero();
        }


       /*-------------------------------------------------------------------------------------------
        * Expr Differentiator::power_derivative(Expr const& base, Expr const& exponent)
        *
        * Derivative of base^exponent. With a constant exponent n this is n*base^(n-1)*base';
        * otherwise base^exponent*(exponent'*log(base) + exponent*base'/base).
        *-----------------------------------------------------------------------------------------*/
        inline Expr Differentiator::power_derivative(Expr const& base, Expr const& exponent)
        {
            Expr base_derivative     = (*this)(base);
            Expr exponent_derivative = (*this)(exponent);

            if (is_zero_expr(exponent_derivative))
            {
                if (is_zero_expr(base_derivative)) return zero();
                Expr lowered = make_pow(base, make_add({exponent, Expr(-1)}));
                return make_mul({exponent, lowered, base_derivative});
            }

            Expr power    = make_pow(base, exponent);
            Expr from_log = make_mul({exponent_derivative, make_call(Elementary::Log, base)});
            Expr from_base = make_mul({exponent, base_derivative, make_pow(base, Expr(-1))});
            return make_mul({power, make_add({from_log, from_base})});
        }


       /*-------------------------------------------------------------------------------------------
        * class Substituter
        *
        * Replaces variables in expressions, rebuilding the changed parts in canonical form.
        * Each node's result is remembered, so shared subexpressions are handled once, and parts
        * that contain none of the variables are returned as they are (sharing their nodes).
        *-----------------------------------------------------------------------------------------*/
        class Substituter
        {
          private:
            std::unordered_map<std::uint64_t, Expr> replacements_; // By variable identifier
            std::unordered_map<Node const*, Expr>   results_;      // Results found so far

          public:
            // Throws std::invalid_argument if a variable is listed twice
            explicit Substituter(Substitution const& replacements);

            // The expression with the variables replaced
            Expr operator()(Expr const& expr);

          private:
            // Result for an expression not yet seen
            Expr compute(Expr const& expr);
        };


       /*-------------------------------------------------------------------------------------------
        * Substituter::Substituter(Substitution const& replacements)
        *
        * Indexes the replacements by variable; throws std::invalid_argument if a variable is
        * listed twice.
        *-----------------------------------------------------------------------------------------*/
        inline Substituter::Substituter(Substitution const& replacements)
        {
            for (auto const& [variable, replacement] : replacements)
            {
                if (!this->replacements_.try_emplace(variable.id(), replacement).second)
                {
                    throw std::invalid_argument("substitute: '" + variable.name()
                                                + "' is given more than once");
                }
            }
        }


       /*-------------------------------------------------------------------------------------------
        * Expr Substituter::operator()(Expr const& expr)
        *
        * The expression with the variables replaced, computed once per node.
        *-----------------------------------------------------------------------------------------*/
        inline Expr Substituter::operator()(Expr const& expr)
        {
            auto it = this->results_.find(&expr.node());
            if (it != this->results_.end()) return it->second;
            Expr result = this->compute(expr);
            this->results_.emplace(&expr.node(), result);
            return result;
        }


       /*-------------------------------------------------------------------------------------------
        * Expr Substituter::compute(Expr const& expr)
        *
        * Replaces variables in the children, then rebuilds the expression if any child changed.
        *-----------------------------------------------------------------------------------------*/
        inline Expr Substituter::compute(Expr const& expr)
        {
            switch (expr.kind())
            {
                case Kind::Number:
                    return expr;

                case Kind::Symbol:
                {
                    auto it = this->replacements_.find(expr.node().symbol().id);
                    return it == this->replacements_.end() ? expr : it->second;
                }

                case Kind::Add:
                {
                    AddData const& sum = expr.node().add();
                    std::vector<Expr> operands{Expr(sum.constant)};
                    bool changed = false;
                    for (Term const& term : sum.terms)
                    {
                        Expr replaced = (*this)(term.expr);
                        changed = changed || !same_node(replaced, term.expr);
                        operands.push_back(make_mul({Expr(term.coefficient), replaced}));
                    }
                    return changed ? make_add(operands) : expr;
                }

                case Kind::Mul:
                {
                    MulData const& product = expr.node().mul();
                    std::vector<Expr> operands{Expr(product.coefficient)};
                    bool changed = false;
                    for (Factor const& factor : product.factors)
                    {
                        Expr base     = (*this)(factor.base);
                        Expr exponent = (*this)(factor.exponent);
                        changed = changed || !same_node(base, factor.base)
                                          || !same_node(exponent, factor.exponent);
                        operands.push_back(make_pow(base, exponent));
                    }
                    return changed ? make_mul(operands) : expr;
                }

                case Kind::Pow:
                {
                    PowData const& power = expr.node().pow();
                    Expr base     = (*this)(power.base);
                    Expr exponent = (*this)(power.exponent);
                    bool changed  = !same_node(base, power.base)
                                 || !same_node(exponent, power.exponent);
                    return changed ? make_pow(base, exponent) : expr;
                }

                case Kind::Call:
                {
                    CallData const& call = expr.node().call();
                    Expr argument = (*this)(call.argument);
                    if (same_node(argument, call.argument)) return expr;
                    return make_call(call.function, argument);
                }
            }
            return expr;
        }
    }


   /*-----------------------------------------------------------------------------------------------
    * Expr diff(Expr const& expr, Symbol const& variable, int order)
    *
    * The derivative of an expression with respect to a variable.
    *
    * Parameters:
    *     expr        Expression to differentiate
    *     variable    Variable to differentiate with respect to
    *     order       Number of times to differentiate (0 gives the expression itself)
    *
    * Returns:
    *     The derivative, in canonical form
    *---------------------------------------------------------------------------------------------*/
    inline Expr diff(Expr const& expr, Symbol const& variable, int order)
    {
        if (order < 0)
        {
            throw std::invalid_argument("diff: the order must be at least 0, not "
                                        + std::to_string(order));
        }
        Expr result = expr;
        for (int i = 0; i < order; ++i)
        {
            detail::Differentiator differentiate(variable);
            result = differentiate(result);
        }
        return result;
    }


   /*-----------------------------------------------------------------------------------------------
    * Expr substitute(Expr const& expr, Substitution const& replacements)
    *
    * Replaces variables by expressions, all at once, and rebuilds the result in canonical form.
    *---------------------------------------------------------------------------------------------*/
    inline Expr substitute(Expr const& expr, Substitution const& replacements)
    {
        detail::Substituter replace(replacements);
        return replace(expr);
    }


   /*-----------------------------------------------------------------------------------------------
    * std::vector<Symbol> symbols(Expr const& expr)
    *
    * The distinct variables in an expression, in canonical order.
    *---------------------------------------------------------------------------------------------*/
    inline std::vector<Symbol> symbols(Expr const& expr)
    {
        std::vector<Symbol>                     found;
        std::unordered_set<detail::Node const*> visited;
        std::vector<Expr>                       pending{expr};
        while (!pending.empty())
        {
            Expr next = pending.back();
            pending.pop_back();
            if (!visited.insert(&next.node()).second) continue;
            if (next.is_symbol()) found.push_back(Symbol::from(next));
            for (Expr const& child : detail::children(next)) pending.push_back(child);
        }
        std::sort(found.begin(), found.end(), detail::ExprLess{});
        return found;
    }
}
#endif
