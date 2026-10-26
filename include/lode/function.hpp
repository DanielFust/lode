/*--------------------------------------------------------------------------------------------------
 * File: function.hpp
 *
 * Description: Functions: expressions bound to an ordered domain of variables and compiled for
 *              fast evaluation, with derivatives and partial evaluation.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_FUNCTION_HPP
#define LODE_FUNCTION_HPP

#include <array>
#include <concepts>
#include <cstddef>
#include <memory>
#include <ostream>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "lode/expr.hpp"
#include "lode/detail/program.hpp"
#include "lode/detail/compiler.hpp"

namespace lode
{
   /*-----------------------------------------------------------------------------------------------
    * class Function
    *
    * A function of an ordered list of variables (its domain), with a scalar, vector, matrix or
    * higher-rank array of outputs. The outputs are expressions; they are compiled together when
    * the function is built, so that evaluation runs a flat list of instructions in which shared
    * subexpressions are computed once.
    *
    *     lode::Symbol x("x"), y("y");
    *     lode::Function f(x, y);
    *     f = x*x*y + sin(y);
    *     double value = f(1.0, 2.0);       // evaluate
    *     lode::Function g = f.grad();      // vector of partial derivatives
    *     lode::Function h = f(x, 2);       // partial evaluation: h(x) = f(x, 2)
    *
    * Outputs are stored in row-major order: for shape {m, n}, output (i, j) is at i*n + j.
    * Copies share the compiled program, so copying is cheap, and a function can be evaluated from
    * several threads at once.
    *---------------------------------------------------------------------------------------------*/
    class Function
    {
      private:
        std::vector<Symbol>                    domain_;  // Variables, in input order
        std::vector<Expr>                      outputs_; // Output expressions, row-major
        std::vector<std::size_t>               shape_;   // Dimensions; empty for a scalar
        std::shared_ptr<detail::Program const> program_; // Compiled outputs

      public:
        /*------------------------------------- Construction -------------------------------------*/

        // The constant 0, of no variables
        Function() : Function(std::vector<Symbol>{}, Expr(0)) {}

        // The constant 0 over the given variables; assign an expression to define it
        template<class... SYMBOLS>
            requires (sizeof...(SYMBOLS) > 0 && (std::same_as<SYMBOLS, Symbol> && ...))
        explicit Function(SYMBOLS const&... domain)
            : Function(std::vector<Symbol>{domain...}, Expr(0)) {}

        // A scalar function
        Function(std::vector<Symbol> domain, Expr const& output);

        // A vector function, one output per expression
        Function(std::vector<Symbol> domain, std::vector<Expr> outputs);

        // A function with outputs of the given shape (outputs in row-major order)
        Function(std::vector<Symbol> domain, std::vector<Expr> outputs,
                 std::vector<std::size_t> shape);

        // Redefines the function as a scalar expression over the same domain. Only on a named
        // function: f(x, y) = ... would assign to a temporary and is not allowed.
        Function& operator=(Expr const& output) &;

        /*--------------------------------------- Queries ----------------------------------------*/

        std::vector<Symbol> const& domain() const {return this->domain_;}
        std::vector<Expr> const& outputs() const {return this->outputs_;}
        std::vector<std::size_t> const& shape() const {return this->shape_;}

        // Number of inputs and of outputs
        std::size_t input_size() const {return this->domain_.size();}
        std::size_t size() const {return this->outputs_.size();}

        bool is_scalar() const {return this->shape_.empty();}

        // The expression of a scalar function; throws std::logic_error otherwise
        Expr const& expr() const;

        // The slice of the outputs at an index of the first dimension: row i of a matrix
        // function, or entry i of a vector function. Throws std::logic_error for a scalar and
        // std::out_of_range for a bad index.
        Function operator[](std::size_t index) const;

        // Text form of the outputs: "x*y", "[x, y]", "[[1, 0], [0, 1]]"
        std::string to_string() const;

        // The compiled program, one instruction per line, and its size
        std::string listing() const {return this->program_->listing();}
        std::size_t instruction_count() const {return this->program_->instructions().size();}
        std::size_t register_count() const {return this->program_->register_count();}

        /*-------------------------------------- Evaluation --------------------------------------*/

        // Value of a scalar function at a point, given one number per variable. Throws
        // std::invalid_argument for the wrong number of arguments, std::logic_error if the
        // function is not scalar.
        template<class... ARGS>
            requires (std::is_arithmetic_v<ARGS> && ...)
        double operator()(ARGS... args) const;

        // All outputs at one point. Throws std::invalid_argument if a size is wrong.
        template<class T = double>
        void evaluate(std::span<std::type_identity_t<T> const> inputs,
                      std::span<std::type_identity_t<T>> outputs) const;

        // All outputs at one point, returned
        std::vector<double> evaluate(std::span<double const> inputs) const;

        // All outputs at many points: inputs and outputs are stored point by point
        // (inputs[p*input_size() + i], outputs[p*size() + j]). Throws std::invalid_argument if
        // the sizes do not describe the same number of points.
        template<class T = double>
        void evaluate_batch(std::span<std::type_identity_t<T> const> inputs,
                            std::span<std::type_identity_t<T>> outputs) const;

        /*---------------------------------- Partial evaluation ----------------------------------*/

        // The function with its variables replaced by the arguments, one per variable: numbers
        // fix a variable, symbols keep or rename one, and expressions reparametrize. The new
        // domain is the variables of the arguments, in order of first appearance, so for
        // f(x, y, z), f(x, 2, z) is a function of (x, z) and f(2*t, y, z) one of (t, y, z).
        // An integer argument stays exact; a floating-point one becomes a real.
        template<class... ARGS>
            requires (!(std::is_arithmetic_v<ARGS> && ...)
                      && (std::convertible_to<ARGS const&, Expr> && ...))
        Function operator()(ARGS const&... args) const {return this->bind({Expr(args)...});}

        // The function with some variables replaced; they leave the domain, and variables of
        // the replacements that are new to the domain are added at its end
        Function partial(Substitution const& replacements) const;

        /*------------------------------------- Derivatives --------------------------------------*/

        // The derivative of every output with respect to one variable (same shape)
        Function diff(Symbol const& variable) const;

        // The derivative of every output with respect to every variable: the shape gains a last
        // dimension of input_size(). A scalar gives its gradient, a vector its Jacobian matrix.
        Function grad() const;

        // The second derivatives: grad().grad(), the Hessian matrix of a scalar function
        Function hessian() const {return this->grad().grad();}

      private:
        // Checks the shape and compiles the outputs
        void build();

        // The function with each variable replaced by the matching argument
        Function bind(std::vector<Expr> const& arguments) const;

        // Text of the outputs from position first, with the given dimensions
        std::string text(std::size_t first, std::size_t dimension) const;
    };

    // Writes the function's text form
    std::ostream& operator<<(std::ostream& out, Function const& function);


   /*-----------------------------------------------------------------------------------------------
    * Function::Function(std::vector<Symbol> domain, Expr const& output)
    *
    * A scalar function. Throws std::invalid_argument if a variable repeats in the domain or the
    * output uses a variable outside it.
    *---------------------------------------------------------------------------------------------*/
    inline Function::Function(std::vector<Symbol> domain, Expr const& output)
        : domain_(std::move(domain)), outputs_{output}
    {this->build();}


   /*-----------------------------------------------------------------------------------------------
    * Function::Function(std::vector<Symbol> domain, std::vector<Expr> outputs)
    *
    * A vector function with one output per expression.
    *---------------------------------------------------------------------------------------------*/
    inline Function::Function(std::vector<Symbol> domain, std::vector<Expr> outputs)
        : domain_(std::move(domain)), outputs_(std::move(outputs)), shape_{outputs_.size()}
    {this->build();}


   /*-----------------------------------------------------------------------------------------------
    * Function::Function(std::vector<Symbol> domain, std::vector<Expr> outputs,
    *                    std::vector<std::size_t> shape)
    *
    * A function with outputs of any shape. Throws std::invalid_argument if the number of outputs
    * is not the product of the dimensions.
    *---------------------------------------------------------------------------------------------*/
    inline Function::Function(std::vector<Symbol> domain, std::vector<Expr> outputs,
                              std::vector<std::size_t> shape)
        : domain_(std::move(domain)), outputs_(std::move(outputs)), shape_(std::move(shape))
    {this->build();}


   /*-----------------------------------------------------------------------------------------------
    * Function& Function::operator=(Expr const& output) &
    *
    * Redefines the function as a scalar expression over the same domain.
    *---------------------------------------------------------------------------------------------*/
    inline Function& Function::operator=(Expr const& output) &
    {
        this->outputs_ = {output};
        this->shape_.clear();
        this->build();
        return *this;
    }


   /*-----------------------------------------------------------------------------------------------
    * void Function::build()
    *
    * Checks that the shape matches the outputs, then compiles them.
    *---------------------------------------------------------------------------------------------*/
    inline void Function::build()
    {
        std::size_t count = 1;
        for (std::size_t dimension : this->shape_) count *= dimension;
        if (count != this->outputs_.size())
        {
            throw std::invalid_argument("Function: the shape holds " + std::to_string(count)
                                        + " outputs, but " + std::to_string(this->outputs_.size())
                                        + " were given");
        }
        detail::Compiler compiler(this->domain_);
        this->program_ = compiler.compile(this->outputs_);
    }


   /*-----------------------------------------------------------------------------------------------
    * Expr const& Function::expr() const
    *
    * The expression of a scalar function; throws std::logic_error otherwise.
    *---------------------------------------------------------------------------------------------*/
    inline Expr const& Function::expr() const
    {
        if (!this->is_scalar())
            throw std::logic_error("Function::expr: the function is not scalar: " + this->to_string());
        return this->outputs_.front();
    }


   /*-----------------------------------------------------------------------------------------------
    * Function Function::operator[](std::size_t index) const
    *
    * The outputs at one index of the first dimension, as a function of one rank lower.
    *---------------------------------------------------------------------------------------------*/
    inline Function Function::operator[](std::size_t index) const
    {
        if (this->is_scalar()) throw std::logic_error("Function: a scalar function has no entries");
        if (index >= this->shape_.front())
        {
            throw std::out_of_range("Function: index " + std::to_string(index)
                                    + " is out of range for dimension "
                                    + std::to_string(this->shape_.front()));
        }
        std::size_t stride = this->outputs_.size() / this->shape_.front();
        auto first = this->outputs_.begin() + static_cast<std::ptrdiff_t>(index * stride);
        std::vector<Expr> outputs(first, first + static_cast<std::ptrdiff_t>(stride));
        std::vector<std::size_t> shape(this->shape_.begin() + 1, this->shape_.end());
        return Function(this->domain_, std::move(outputs), std::move(shape));
    }


   /*-----------------------------------------------------------------------------------------------
    * std::string Function::text(std::size_t first, std::size_t dimension) const
    *
    * Text of the block of outputs starting at position first, spanning dimensions
    * shape_[dimension] onward, as nested brackets.
    *---------------------------------------------------------------------------------------------*/
    inline std::string Function::text(std::size_t first, std::size_t dimension) const
    {
        if (dimension == this->shape_.size()) return this->outputs_[first].to_string();

        std::size_t stride = 1;
        for (std::size_t d = dimension + 1; d < this->shape_.size(); ++d) stride *= this->shape_[d];

        std::string result = "[";
        for (std::size_t i = 0; i < this->shape_[dimension]; ++i)
            result += (i == 0 ? "" : ", ") + this->text(first + i * stride, dimension + 1);
        return result + "]";
    }

    inline std::string Function::to_string() const {return this->text(0, 0);}

    inline std::ostream& operator<<(std::ostream& out, Function const& function)
    {return out << function.to_string();}


   /*-----------------------------------------------------------------------------------------------
    * double Function::operator()(ARGS... args) const
    *
    * Value of a scalar function at a point.
    *
    * Template Parameters:
    *     ARGS      Arithmetic types, one per variable
    *---------------------------------------------------------------------------------------------*/
    template<class... ARGS>
        requires (std::is_arithmetic_v<ARGS> && ...)
    double Function::operator()(ARGS... args) const
    {
        if (!this->is_scalar())
        {
            throw std::logic_error("Function: call evaluate() for a function with "
                                   + std::to_string(this->size()) + " outputs");
        }
        std::array<double, sizeof...(ARGS)> inputs{static_cast<double>(args)...};
        double result = 0.0;
        this->evaluate<double>(inputs, std::span<double>(&result, 1));
        return result;
    }


   /*-----------------------------------------------------------------------------------------------
    * void Function::evaluate(std::span<T const> inputs, std::span<T> outputs) const
    *
    * All outputs at one point.
    *
    * Template Parameters:
    *     T          Number type (double unless given)
    *
    * Parameters:
    *     inputs     One value per variable, in domain order
    *     outputs    One value per output, row-major, written
    *---------------------------------------------------------------------------------------------*/
    template<class T>
    void Function::evaluate(std::span<std::type_identity_t<T> const> inputs,
                            std::span<std::type_identity_t<T>> outputs) const
    {
        if (inputs.size() != this->input_size())
        {
            throw std::invalid_argument("Function: expected " + std::to_string(this->input_size())
                                        + " inputs, got " + std::to_string(inputs.size()));
        }
        if (outputs.size() != this->size())
        {
            throw std::invalid_argument("Function: expected room for " + std::to_string(this->size())
                                        + " outputs, got " + std::to_string(outputs.size()));
        }
        this->program_->run(inputs.data(), outputs.data());
    }

    inline std::vector<double> Function::evaluate(std::span<double const> inputs) const
    {
        std::vector<double> outputs(this->size());
        this->evaluate<double>(inputs, outputs);
        return outputs;
    }


   /*-----------------------------------------------------------------------------------------------
    * void Function::evaluate_batch(std::span<T const> inputs, std::span<T> outputs) const
    *
    * All outputs at many points, evaluated a block of points at a time.
    *
    * Template Parameters:
    *     T          Number type (double unless given)
    *
    * Parameters:
    *     inputs     input_size() values per point, point by point
    *     outputs    size() values per point, point by point, written
    *---------------------------------------------------------------------------------------------*/
    template<class T>
    void Function::evaluate_batch(std::span<std::type_identity_t<T> const> inputs,
                                  std::span<std::type_identity_t<T>> outputs) const
    {
        std::size_t const n_in  = this->input_size();
        std::size_t const n_out = this->size();
        std::size_t count = n_in > 0 ? inputs.size() / n_in : (n_out > 0 ? outputs.size() / n_out : 0);
        if (count * n_in != inputs.size() || count * n_out != outputs.size())
        {
            throw std::invalid_argument("Function: " + std::to_string(inputs.size())
                                        + " inputs and " + std::to_string(outputs.size())
                                        + " outputs do not describe whole points of "
                                        + std::to_string(n_in) + " inputs and "
                                        + std::to_string(n_out) + " outputs");
        }
        this->program_->run_batch(inputs.data(), outputs.data(), count);
    }


   /*-----------------------------------------------------------------------------------------------
    * Function Function::bind(std::vector<Expr> const& arguments) const
    *
    * Replaces each variable by the matching argument. The new domain is the variables of the
    * arguments, in order of first appearance (each argument's own variables in canonical order).
    *---------------------------------------------------------------------------------------------*/
    inline Function Function::bind(std::vector<Expr> const& arguments) const
    {
        if (arguments.size() != this->input_size())
        {
            throw std::invalid_argument("Function: expected " + std::to_string(this->input_size())
                                        + " arguments, got " + std::to_string(arguments.size()));
        }

        std::vector<Symbol> domain;
        for (Expr const& argument : arguments)
        {
            for (Symbol const& variable : symbols(argument))
            {
                bool seen = false;
                for (Symbol const& known : domain) seen = seen || known == variable;
                if (!seen) domain.push_back(variable);
            }
        }

        Substitution replacements;
        for (std::size_t i = 0; i < arguments.size(); ++i)
        {
            if (arguments[i] != this->domain_[i])
                replacements.emplace_back(this->domain_[i], arguments[i]);
        }

        std::vector<Expr> outputs;
        outputs.reserve(this->outputs_.size());
        for (Expr const& output : this->outputs_)
            outputs.push_back(lode::substitute(output, replacements));
        return Function(std::move(domain), std::move(outputs), this->shape_);
    }


   /*-----------------------------------------------------------------------------------------------
    * Function Function::partial(Substitution const& replacements) const
    *
    * Replaces some variables. They leave the domain; variables of the replacements that are not
    * already in the domain are appended, in order of first appearance.
    *---------------------------------------------------------------------------------------------*/
    inline Function Function::partial(Substitution const& replacements) const
    {
        auto is_replaced = [&](Symbol const& variable)
        {
            for (auto const& [replaced, value] : replacements)
                if (replaced == variable) return true;
            return false;
        };

        std::vector<Symbol> domain;
        for (Symbol const& variable : this->domain_)
            if (!is_replaced(variable)) domain.push_back(variable);
        for (auto const& [replaced, value] : replacements)
        {
            for (Symbol const& variable : symbols(value))
            {
                bool seen = false;
                for (Symbol const& known : domain) seen = seen || known == variable;
                if (!seen) domain.push_back(variable);
            }
        }

        std::vector<Expr> outputs;
        outputs.reserve(this->outputs_.size());
        for (Expr const& output : this->outputs_)
            outputs.push_back(lode::substitute(output, replacements));
        return Function(std::move(domain), std::move(outputs), this->shape_);
    }


   /*-----------------------------------------------------------------------------------------------
    * Function Function::diff(Symbol const& variable) const
    *
    * Every output differentiated with respect to one variable.
    *---------------------------------------------------------------------------------------------*/
    inline Function Function::diff(Symbol const& variable) const
    {
        std::vector<Expr> outputs;
        outputs.reserve(this->outputs_.size());
        for (Expr const& output : this->outputs_) outputs.push_back(lode::diff(output, variable));
        return Function(this->domain_, std::move(outputs), this->shape_);
    }


   /*-----------------------------------------------------------------------------------------------
    * Function Function::grad() const
    *
    * Every output differentiated with respect to every variable; output k's derivatives are
    * entries k*input_size() to (k + 1)*input_size() - 1 of the result.
    *---------------------------------------------------------------------------------------------*/
    inline Function Function::grad() const
    {
        std::vector<Expr> outputs;
        outputs.reserve(this->outputs_.size() * this->domain_.size());
        for (Expr const& output : this->outputs_)
        {
            for (Symbol const& variable : this->domain_)
                outputs.push_back(lode::diff(output, variable));
        }

        std::vector<std::size_t> shape = this->shape_;
        shape.push_back(this->domain_.size());
        return Function(this->domain_, std::move(outputs), std::move(shape));
    }
}
#endif
