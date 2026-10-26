/*--------------------------------------------------------------------------------------------------
 * File: node.hpp
 *
 * Description: The immutable nodes that expressions share, and what each kind holds. Included by
 *              expr.hpp; not meant to be included on its own.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_DETAIL_NODE_HPP
#define LODE_DETAIL_NODE_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace lode::detail
{
    // A term of a sum: coefficient*expr
    struct Term
    {
        Expr   expr;
        Number coefficient;
    };

    // A factor of a product: base^exponent
    struct Factor
    {
        Expr base;
        Expr exponent;
    };

    // A variable
    struct SymbolData
    {
        std::string   name;
        std::uint64_t id;
    };

    // constant + sum of terms. Terms are sorted by expr in canonical order, with distinct exprs
    // and nonzero coefficients. A term's expr is never a number or a sum, and if it is a
    // product its coefficient is 1. There are at least two terms, or one term and a nonzero
    // constant.
    struct AddData
    {
        Number            constant;
        std::vector<Term> terms;
    };

    // coefficient * product of factors. Factors are sorted by base in canonical order, with
    // distinct bases and nonzero exponents. A base that is a number, product or power appears
    // only with a non-integer exponent, as in 2^(1/2). A sum with an integer exponent is
    // primitive (see split_content). The coefficient is nonzero, and there are at least two
    // factors, or one factor and a coefficient other than 1. A lone factor s^1 with s a sum does
    // not occur: the coefficient is distributed over the sum instead.
    struct MulData
    {
        Number              coefficient;
        std::vector<Factor> factors;
    };

    // base^exponent, when it does not simplify further
    struct PowData
    {
        Expr base;
        Expr exponent;
    };


   /*-----------------------------------------------------------------------------------------------
    * class Node
    *
    * An immutable expression node: the contents for one kind of expression, and their hash.
    *---------------------------------------------------------------------------------------------*/
    class Node
    {
      public:
        using Data = std::variant<Number, SymbolData, AddData, MulData, PowData>;

      private:
        Data        data_;
        std::size_t hash_;

      public:
        // Node holding the given contents
        template<class DATA>
        explicit Node(DATA data) : data_(std::in_place_type<DATA>, std::move(data)), hash_(0)
        {this->hash_ = this->compute_hash();}

        Kind kind() const {return static_cast<Kind>(this->data_.index());}
        std::size_t hash() const {return this->hash_;}

        // Contents; each throws std::bad_variant_access for a node of another kind
        Number const& number() const {return std::get<Number>(this->data_);}
        SymbolData const& symbol() const {return std::get<SymbolData>(this->data_);}
        AddData const& add() const {return std::get<AddData>(this->data_);}
        MulData const& mul() const {return std::get<MulData>(this->data_);}
        PowData const& pow() const {return std::get<PowData>(this->data_);}

      private:
        std::size_t compute_hash() const;
    };

    // Kind must index the alternatives of Node::Data
    static_assert(std::is_same_v<std::variant_alternative_t<static_cast<std::size_t>(Kind::Number),
                                                            Node::Data>, Number>);
    static_assert(std::is_same_v<std::variant_alternative_t<static_cast<std::size_t>(Kind::Symbol),
                                                            Node::Data>, SymbolData>);
    static_assert(std::is_same_v<std::variant_alternative_t<static_cast<std::size_t>(Kind::Add),
                                                            Node::Data>, AddData>);
    static_assert(std::is_same_v<std::variant_alternative_t<static_cast<std::size_t>(Kind::Mul),
                                                            Node::Data>, MulData>);
    static_assert(std::is_same_v<std::variant_alternative_t<static_cast<std::size_t>(Kind::Pow),
                                                            Node::Data>, PowData>);


   /*-----------------------------------------------------------------------------------------------
    * std::size_t Node::compute_hash() const
    *
    * Hash of the kind and contents, built from the children's stored hashes.
    *---------------------------------------------------------------------------------------------*/
    inline std::size_t Node::compute_hash() const
    {
        std::size_t seed = this->data_.index();
        switch (this->kind())
        {
            case Kind::Number:
                hash_combine(seed, this->number().hash());
                break;
            case Kind::Symbol:
                hash_combine(seed, std::hash<std::uint64_t>{}(this->symbol().id));
                break;
            case Kind::Add:
                hash_combine(seed, this->add().constant.hash());
                for (Term const& term : this->add().terms)
                {
                    hash_combine(seed, term.expr.hash());
                    hash_combine(seed, term.coefficient.hash());
                }
                break;
            case Kind::Mul:
                hash_combine(seed, this->mul().coefficient.hash());
                for (Factor const& factor : this->mul().factors)
                {
                    hash_combine(seed, factor.base.hash());
                    hash_combine(seed, factor.exponent.hash());
                }
                break;
            case Kind::Pow:
                hash_combine(seed, this->pow().base.hash());
                hash_combine(seed, this->pow().exponent.hash());
                break;
        }
        return seed;
    }


    // Builds an expression from node contents, as given (no simplification)
    template<class DATA>
    Expr make_expr(DATA data)
    {return Expr(std::make_shared<Node const>(std::move(data)));}

    // Shared nodes for the constants used most often
    inline Expr const& zero()
    {
        static Expr const value = make_expr(Number(0));
        return value;
    }

    inline Expr const& one()
    {
        static Expr const value = make_expr(Number(1));
        return value;
    }

    // Source of unique variable identifiers
    inline std::atomic<std::uint64_t> next_symbol_id{1};

    // Node for a new variable; throws std::invalid_argument if the name is empty
    inline std::shared_ptr<Node const> new_symbol_node(std::string name)
    {
        if (name.empty()) throw std::invalid_argument("Symbol: the name must not be empty");
        return std::make_shared<Node const>(SymbolData{std::move(name), next_symbol_id++});
    }
}
#endif
