/*--------------------------------------------------------------------------------------------------
 * File: expr_impl.hpp
 *
 * Description: Definitions of the Expr and Symbol members and the expression operators.
 *              Included by expr.hpp; not meant to be included on its own.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_DETAIL_EXPR_IMPL_HPP
#define LODE_DETAIL_EXPR_IMPL_HPP

#include <compare>
#include <cstdint>
#include <memory>
#include <ostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace lode
{
    /*------------------------------------------- Expr -------------------------------------------*/

    inline Expr::Expr() : Expr(detail::zero()) {}

    inline Expr::Expr(Number const& value) : node_(std::make_shared<detail::Node const>(value)) {}

    inline Kind Expr::kind() const {return this->node_->kind();}

    inline std::size_t Expr::hash() const {return this->node_->hash();}

    inline std::string Expr::to_string() const {return detail::print(*this);}


   /*-----------------------------------------------------------------------------------------------
    * Number const& Expr::number() const
    *
    * Value of a number; throws std::logic_error for any other expression.
    *---------------------------------------------------------------------------------------------*/
    inline Number const& Expr::number() const
    {
        if (!this->is_number())
            throw std::logic_error("Expr::number: '" + this->to_string() + "' is not a number");
        return this->node_->number();
    }

    /*------------------------------------------ Symbol ------------------------------------------*/

    inline Symbol::Symbol(std::string name) : Expr(detail::new_symbol_node(std::move(name))) {}

    inline std::string const& Symbol::name() const {return this->node().symbol().name;}

    inline std::uint64_t Symbol::id() const {return this->node().symbol().id;}

    /*---------------------------------------- Operators -----------------------------------------*/

    inline Expr operator+(Expr const& a, Expr const& b) {return detail::make_add({a, b});}

    inline Expr operator-(Expr const& a, Expr const& b)
    {return detail::make_add({a, detail::make_mul({Expr(-1), b})});}

    inline Expr operator*(Expr const& a, Expr const& b) {return detail::make_mul({a, b});}

    // Throws std::domain_error if b is zero
    inline Expr operator/(Expr const& a, Expr const& b)
    {return detail::make_mul({a, detail::make_pow(b, Expr(-1))});}

    inline Expr operator-(Expr const& a) {return detail::make_mul({Expr(-1), a});}

    inline Expr pow(Expr const& base, Expr const& exponent) {return detail::make_pow(base, exponent);}

    inline Expr sqrt(Expr const& value) {return detail::make_pow(value, Expr(Number(1, 2)));}


   /*-----------------------------------------------------------------------------------------------
    * bool operator==(Expr const& a, Expr const& b)
    *
    * Whether two expressions are the same. Since both are in canonical form, this compares their
    * structure, after quick checks of identity and hash.
    *---------------------------------------------------------------------------------------------*/
    inline bool operator==(Expr const& a, Expr const& b)
    {
        if (&a.node() == &b.node()) return true;
        if (a.hash() != b.hash()) return false;
        return std::is_eq(compare(a, b));
    }

    inline std::ostream& operator<<(std::ostream& out, Expr const& expr)
    {return out << expr.to_string();}
}
#endif
