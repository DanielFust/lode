/*--------------------------------------------------------------------------------------------------
 * File: function.hpp
 *
 * Description: Functions: expressions bound to a domain of variables and compiled for fast
 *              evaluation, with derivatives and partial evaluation.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_FUNCTION_HPP
#define LODE_FUNCTION_HPP

#include <array>
#include <concepts>
#include <cstddef>
#include <initializer_list>
#include <memory>
#include <ostream>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "lode/expr.hpp"
#include "lode/tensor.hpp"
#include "lode/detail/program.hpp"
#include "lode/detail/compiler.hpp"

namespace lode
{
   /*-----------------------------------------------------------------------------------------------
    * class Parameters
    *
    * Variables that a Function takes as inputs but does not differentiate with respect to, such
    * as a temperature or a rate constant. Their values are given at each evaluation, so they can
    * change freely without rebuilding the function; partial evaluation can still fix them.
    *---------------------------------------------------------------------------------------------*/
    class Parameters
    {
      private:
        std::vector<Symbol> symbols_;

      public:
        Parameters() = default;

        // Implicit, so that a braced list of symbols can be passed: {temperature, rate}
        Parameters(std::initializer_list<Symbol> symbols) : symbols_(symbols) {}

        explicit Parameters(std::vector<Symbol> symbols) : symbols_(std::move(symbols)) {}

        std::vector<Symbol> const& symbols() const {return this->symbols_;}
    };


    namespace detail
    {
        // Appends the variables of an expression that are in neither list, in canonical order
        inline void append_new_symbols(std::vector<Symbol>& list, Expr const& expr,
                                       std::vector<Symbol> const& exclude = {})
        {
            for (Symbol const& variable : symbols(expr))
            {
                bool known = false;
                for (Symbol const& other : list) known = known || other == variable;
                for (Symbol const& other : exclude) known = known || other == variable;
                if (!known) list.push_back(variable);
            }
        }
    }


   /*-----------------------------------------------------------------------------------------------
    * class Function
    *
    * A function of a domain of variables, with outputs that are a scalar, a vector, a matrix or
    * a tensor of higher rank. The outputs are expressions, compiled together when the function
    * is built, so that evaluation runs a flat list of instructions in which shared
    * subexpressions are computed once.
    *
    *     lode::Symbol x("x"), y("y");
    *     lode::Function f(x, y);
    *     f = x*x*y + sin(y);
    *     double value = f(1.0, 2.0);           // evaluate
    *     lode::Function g = f.grad();          // the vector of partial derivatives
    *     lode::Expr dg2 = g.outputs()(2);      // its second entry, counted from 1
    *     lode::Function h = f(x, 2);           // partial evaluation: h(x) = f(x, 2)
    *
    * The domain is usually a list of variables, but it can be any tensor of them, such as a 3x3
    * matrix variable (see make_symbols). The gradient appends the domain's shape to the
    * outputs' shape, so a scalar function of a matrix has a matrix gradient. The derivative
    * index comes last: the Jacobian of a vector function F is J(i, j) = dF_i/dx_j.
    *
    * Functions are applied, with (); their outputs and values are tensors, indexed with () from
    * 1. Copies share the compiled program, so copying is cheap, and a function can be evaluated
    * from several threads at once.
    *
    * A function may also have parameters (see Parameters). Its inputs are then the domain's
    * variables, in storage order, followed by the parameters: calls and evaluate take values for
    * all inputs, in that order, while grad differentiates with respect to the domain only.
    *---------------------------------------------------------------------------------------------*/
    class Function
    {
      private:
        Tensor<Symbol>                         domain_;     // Variables of the domain
        std::vector<Symbol>                    parameters_; // Inputs after the domain
        Tensor<Expr>                           outputs_;    // Output expressions
        std::shared_ptr<detail::Program const> program_;    // Compiled outputs

      public:
        /*------------------------------------- Construction -------------------------------------*/

        // The constant 0, of no variables
        Function() : Function(std::vector<Symbol>{}, Expr(0)) {}

        // The constant 0 over the given variables; assign an expression to define it
        template<class... SYMBOLS>
            requires (sizeof...(SYMBOLS) > 0 && (std::same_as<SYMBOLS, Symbol> && ...))
        explicit Function(SYMBOLS const&... domain)
            : Function(std::vector<Symbol>{domain...}, Expr(0)) {}

        // A scalar function. The domain is a braced list {x, y}, a std::vector, or any tensor
        // of variables.
        Function(Tensor<Symbol> domain, Expr const& output, Parameters parameters = {});

        // A function with a tensor of outputs: {f, g} for a vector, {{a, b}, {c, d}} for a
        // matrix, or any Tensor<Expr>
        Function(Tensor<Symbol> domain, Tensor<Expr> outputs, Parameters parameters = {});

        // Redefines the function as a scalar expression over the same domain. Only on a named
        // function: f(x, y) = ... would assign to a temporary and is not allowed.
        Function& operator=(Expr const& output) &;

        /*--------------------------------------- Queries ----------------------------------------*/

        Tensor<Symbol> const& domain() const {return this->domain_;}
        std::vector<Symbol> const& parameters() const {return this->parameters_;}
        Tensor<Expr> const& outputs() const {return this->outputs_;}

        // Dimensions of the outputs; empty for a scalar
        std::vector<std::size_t> const& shape() const {return this->outputs_.shape();}

        // Number of inputs (the domain's variables, then the parameters) and of outputs
        std::size_t input_size() const {return this->domain_.size() + this->parameters_.size();}
        std::size_t size() const {return this->outputs_.size();}

        bool is_scalar() const {return this->outputs_.rank() == 0;}

        // The expression of a scalar function; throws std::logic_error otherwise
        Expr const& expr() const;

        // Text form of the outputs: "x*y", "[x, y]", "[[1, 0], [0, 1]]"
        std::string to_string() const;

        // The compiled program, one instruction per line, and its size
        std::string listing() const {return this->program_->listing();}
        std::size_t instruction_count() const {return this->program_->instructions().size();}
        std::size_t register_count() const {return this->program_->register_count();}

        /*-------------------------------------- Evaluation --------------------------------------*/

        // Value of a scalar function at a point, given one number per input. Throws
        // std::invalid_argument for the wrong number of arguments, std::logic_error if the
        // function is not scalar (use evaluate).
        template<class... ARGS>
            requires (std::is_arithmetic_v<ARGS> && ...)
        double operator()(ARGS... args) const;

        // All outputs at one point, as a tensor of the outputs' shape
        Tensor<double> evaluate(std::span<double const> inputs) const;

        // All outputs at one point, written in row-major order. Throws std::invalid_argument if
        // a size is wrong.
        template<class T = double>
        void evaluate(std::span<std::type_identity_t<T> const> inputs,
                      std::span<std::type_identity_t<T>> outputs) const;

        // All outputs at one point, with the values of the domain's variables and of the
        // parameters given separately
        template<class T = double>
        void evaluate(std::span<std::type_identity_t<T> const> point,
                      std::span<std::type_identity_t<T> const> parameters,
                      std::span<std::type_identity_t<T>> outputs) const;

        // All outputs at many points: inputs and outputs are stored point by point
        // (inputs[p*input_size() + i], outputs[p*size() + j]). Throws std::invalid_argument if
        // the sizes do not describe the same number of points.
        template<class T = double>
        void evaluate_batch(std::span<std::type_identity_t<T> const> inputs,
                            std::span<std::type_identity_t<T>> outputs) const;

        // All outputs at many points that share the same parameter values: each point gives
        // values for the domain's variables only (points[p*domain().size() + i])
        template<class T = double>
        void evaluate_batch(std::span<std::type_identity_t<T> const> points,
                            std::span<std::type_identity_t<T> const> parameters,
                            std::span<std::type_identity_t<T>> outputs) const;

        /*---------------------------------- Partial evaluation ----------------------------------*/

        // The function with its inputs replaced by the arguments, one per input: numbers fix an
        // input, symbols keep or rename one, and expressions reparametrize. The new domain is a
        // list of the variables of the arguments for the domain, in order of first appearance,
        // so for f(x, y, z), f(x, 2, z) is a function of (x, z) and f(2*t, y, z) one of
        // (t, y, z). Variables of the arguments for parameters become the new parameters. An
        // integer argument stays exact; a floating-point one becomes a real.
        template<class... ARGS>
            requires (!(std::is_arithmetic_v<ARGS> && ...)
                      && (std::convertible_to<ARGS const&, Expr> && ...))
        Function operator()(ARGS const&... args) const {return this->bind({Expr(args)...});}

        // The function with some inputs replaced; they leave the domain or the parameters, and
        // new variables in a replacement join the list the replaced input belonged to. If no
        // variable of the domain is replaced, the domain keeps its shape.
        Function partial(Substitution const& replacements) const;

        /*------------------------------------- Derivatives --------------------------------------*/

        // The derivative of every output with respect to one variable (same shape)
        Function diff(Symbol const& variable) const;

        // The derivative of every output with respect to every variable of the domain: the
        // outputs' shape followed by the domain's shape. A scalar gives its gradient, a vector
        // its Jacobian matrix. Parameters are not differentiated.
        Function grad() const;

        // The second derivatives: grad().grad(), the Hessian matrix of a scalar function
        Function hessian() const {return this->grad().grad();}

      private:
        // Compiles the outputs
        void build();

        // Checks the sizes of inputs and outputs for one point
        void check_point(std::size_t point_size, std::size_t expected_point,
                         std::size_t parameter_size, std::size_t output_size) const;

        // The function with each input replaced by the matching argument
        Function bind(std::vector<Expr> const& arguments) const;

        // The outputs with a substitution applied to each
        Tensor<Expr> substituted(Substitution const& replacements) const;
    };

    // Writes the function's text form
    std::ostream& operator<<(std::ostream& out, Function const& function);


   /*-----------------------------------------------------------------------------------------------
    * Function::Function(Tensor<Symbol> domain, Expr const& output, Parameters parameters)
    *
    * A scalar function. Throws std::invalid_argument if a variable repeats among the domain and
    * parameters, or the output uses a variable outside them.
    *---------------------------------------------------------------------------------------------*/
    inline Function::Function(Tensor<Symbol> domain, Expr const& output, Parameters parameters)
        : domain_(std::move(domain)), parameters_(parameters.symbols()),
          outputs_(Shape{}, std::vector<Expr>{output})
    {this->build();}


   /*-----------------------------------------------------------------------------------------------
    * Function::Function(Tensor<Symbol> domain, Tensor<Expr> outputs, Parameters parameters)
    *
    * A function with a tensor of outputs.
    *---------------------------------------------------------------------------------------------*/
    inline Function::Function(Tensor<Symbol> domain, Tensor<Expr> outputs, Parameters parameters)
        : domain_(std::move(domain)), parameters_(parameters.symbols()), outputs_(std::move(outputs))
    {this->build();}


   /*-----------------------------------------------------------------------------------------------
    * Function& Function::operator=(Expr const& output) &
    *
    * Redefines the function as a scalar expression over the same domain.
    *---------------------------------------------------------------------------------------------*/
    inline Function& Function::operator=(Expr const& output) &
    {
        this->outputs_ = Tensor<Expr>(Shape{}, std::vector<Expr>{output});
        this->build();
        return *this;
    }


    // Compiles the outputs over the domain's variables, then the parameters
    inline void Function::build()
    {
        detail::Compiler compiler(this->domain_.elements(), this->parameters_);
        this->program_ = compiler.compile(this->outputs_.elements());
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
        return this->outputs_[0];
    }


    inline std::string Function::to_string() const
    {
        std::ostringstream text;
        text << this->outputs_;
        return text.str();
    }

    inline std::ostream& operator<<(std::ostream& out, Function const& function)
    {return out << function.outputs();}


   /*-----------------------------------------------------------------------------------------------
    * double Function::operator()(ARGS... args) const
    *
    * Value of a scalar function at a point.
    *
    * Template Parameters:
    *     ARGS      Arithmetic types, one per input
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
    * Tensor<double> Function::evaluate(std::span<double const> inputs) const
    *
    * All outputs at one point, as a tensor with the outputs' shape.
    *---------------------------------------------------------------------------------------------*/
    inline Tensor<double> Function::evaluate(std::span<double const> inputs) const
    {
        Tensor<double> values(Shape(this->outputs_.shape()));
        this->evaluate<double>(inputs, std::span<double>(values.data(), values.size()));
        return values;
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
    *     inputs     One value per input: the domain's variables, then the parameters
    *     outputs    One value per output, row-major, written
    *---------------------------------------------------------------------------------------------*/
    template<class T>
    void Function::evaluate(std::span<std::type_identity_t<T> const> inputs,
                            std::span<std::type_identity_t<T>> outputs) const
    {
        this->check_point(inputs.size(), this->input_size(), 0, outputs.size());
        this->program_->run(inputs.data(), outputs.data());
    }


   /*-----------------------------------------------------------------------------------------------
    * void Function::evaluate(std::span<T const> point, std::span<T const> parameters,
    *                        std::span<T> outputs) const
    *
    * All outputs at one point, with the parameters' values given separately.
    *
    * Template Parameters:
    *     T             Number type (double unless given)
    *
    * Parameters:
    *     point         One value per variable of the domain, in storage order
    *     parameters    One value per parameter, in order
    *     outputs       One value per output, row-major, written
    *---------------------------------------------------------------------------------------------*/
    template<class T>
    void Function::evaluate(std::span<std::type_identity_t<T> const> point,
                            std::span<std::type_identity_t<T> const> parameters,
                            std::span<std::type_identity_t<T>> outputs) const
    {
        this->check_point(point.size(), this->domain_.size(), parameters.size(), outputs.size());
        this->program_->run(point.data(), point.size(), parameters.data(), outputs.data());
    }


   /*-----------------------------------------------------------------------------------------------
    * void Function::check_point(std::size_t point_size, std::size_t expected_point,
    *                           std::size_t parameter_size, std::size_t output_size) const
    *
    * Checks the sizes for evaluating one point; throws std::invalid_argument naming the
    * mismatch.
    *
    * Parameters:
    *     point_size        Values given for the point
    *     expected_point    Values the point should have: input_size(), or the domain's size
    *                       when parameter values are given separately
    *     parameter_size    Parameter values given separately (0 if none)
    *     output_size       Room given for outputs
    *---------------------------------------------------------------------------------------------*/
    inline void Function::check_point(std::size_t point_size, std::size_t expected_point,
                                      std::size_t parameter_size, std::size_t output_size) const
    {
        if (point_size != expected_point)
        {
            throw std::invalid_argument("Function: expected " + std::to_string(expected_point)
                                        + " inputs, got " + std::to_string(point_size));
        }
        if (point_size + parameter_size != this->input_size())
        {
            throw std::invalid_argument("Function: expected "
                                        + std::to_string(this->parameters_.size())
                                        + " parameter values, got " + std::to_string(parameter_size));
        }
        if (output_size != this->size())
        {
            throw std::invalid_argument("Function: expected room for " + std::to_string(this->size())
                                        + " outputs, got " + std::to_string(output_size));
        }
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
        this->program_->run_batch(inputs.data(), n_in, static_cast<T const*>(nullptr), outputs.data(),
                                  count);
    }


   /*-----------------------------------------------------------------------------------------------
    * void Function::evaluate_batch(std::span<T const> points, std::span<T const> parameters,
    *                              std::span<T> outputs) const
    *
    * All outputs at many points sharing the same parameter values.
    *
    * Template Parameters:
    *     T             Number type (double unless given)
    *
    * Parameters:
    *     points        domain().size() values per point, point by point
    *     parameters    One value per parameter, shared by all points
    *     outputs       size() values per point, point by point, written
    *---------------------------------------------------------------------------------------------*/
    template<class T>
    void Function::evaluate_batch(std::span<std::type_identity_t<T> const> points,
                                  std::span<std::type_identity_t<T> const> parameters,
                                  std::span<std::type_identity_t<T>> outputs) const
    {
        if (parameters.size() != this->parameters_.size())
        {
            throw std::invalid_argument("Function: expected "
                                        + std::to_string(this->parameters_.size())
                                        + " parameter values, got "
                                        + std::to_string(parameters.size()));
        }
        std::size_t const n_in  = this->domain_.size();
        std::size_t const n_out = this->size();
        std::size_t count = n_in > 0 ? points.size() / n_in : (n_out > 0 ? outputs.size() / n_out : 0);
        if (count * n_in != points.size() || count * n_out != outputs.size())
        {
            throw std::invalid_argument("Function: " + std::to_string(points.size())
                                        + " values and " + std::to_string(outputs.size())
                                        + " outputs do not describe whole points of "
                                        + std::to_string(n_in) + " inputs and "
                                        + std::to_string(n_out) + " outputs");
        }
        this->program_->run_batch(points.data(), n_in, parameters.data(), outputs.data(), count);
    }


    // The outputs with a substitution applied to each, keeping their shape
    inline Tensor<Expr> Function::substituted(Substitution const& replacements) const
    {
        std::vector<Expr> outputs;
        outputs.reserve(this->outputs_.size());
        for (Expr const& output : this->outputs_)
            outputs.push_back(lode::substitute(output, replacements));
        return Tensor<Expr>(Shape(this->outputs_.shape()), std::move(outputs));
    }


   /*-----------------------------------------------------------------------------------------------
    * Function Function::bind(std::vector<Expr> const& arguments) const
    *
    * Replaces each input by the matching argument. The new domain is the list of the variables
    * of the arguments for the domain, in order of first appearance (each argument's own
    * variables in canonical order); the new parameters are the other variables of the arguments
    * for the parameters.
    *---------------------------------------------------------------------------------------------*/
    inline Function Function::bind(std::vector<Expr> const& arguments) const
    {
        if (arguments.size() != this->input_size())
        {
            throw std::invalid_argument("Function: expected " + std::to_string(this->input_size())
                                        + " arguments, got " + std::to_string(arguments.size()));
        }

        std::size_t const   n_domain = this->domain_.size();
        std::vector<Symbol> domain;
        std::vector<Symbol> parameters;
        for (std::size_t i = 0; i < n_domain; ++i) detail::append_new_symbols(domain, arguments[i]);
        for (std::size_t i = n_domain; i < arguments.size(); ++i)
            detail::append_new_symbols(parameters, arguments[i], domain);

        Substitution replacements;
        for (std::size_t i = 0; i < arguments.size(); ++i)
        {
            Symbol const& input = i < n_domain ? this->domain_[i] : this->parameters_[i - n_domain];
            if (arguments[i] != input) replacements.emplace_back(input, arguments[i]);
        }
        return Function(std::move(domain), this->substituted(replacements),
                        Parameters(std::move(parameters)));
    }


   /*-----------------------------------------------------------------------------------------------
    * Function Function::partial(Substitution const& replacements) const
    *
    * Replaces some inputs. Each replaced input leaves the domain or the parameters, and the new
    * variables of its replacement join the same list, in order of first appearance. A domain
    * none of whose variables is replaced keeps its shape; otherwise the new domain is a list.
    *---------------------------------------------------------------------------------------------*/
    inline Function Function::partial(Substitution const& replacements) const
    {
        auto contains = [](auto const& list, Symbol const& variable)
        {
            for (Symbol const& other : list)
                if (other == variable) return true;
            return false;
        };
        auto is_replaced = [&](Symbol const& variable)
        {
            for (auto const& [replaced, value] : replacements)
                if (replaced == variable) return true;
            return false;
        };

        std::vector<Symbol> domain;
        std::vector<Symbol> parameters;
        bool domain_changed = false;
        for (Symbol const& variable : this->domain_)
        {
            if (is_replaced(variable))
                domain_changed = true;
            else
                domain.push_back(variable);
        }
        for (Symbol const& variable : this->parameters_)
            if (!is_replaced(variable)) parameters.push_back(variable);
        for (auto const& [replaced, value] : replacements)
        {
            if (contains(this->domain_, replaced))
                detail::append_new_symbols(domain, value, parameters);
            else if (contains(this->parameters_, replaced))
                detail::append_new_symbols(parameters, value, domain);
        }

        Tensor<Symbol> new_domain = domain_changed ? Tensor<Symbol>(std::move(domain)) : this->domain_;
        return Function(std::move(new_domain), this->substituted(replacements),
                        Parameters(std::move(parameters)));
    }


   /*-----------------------------------------------------------------------------------------------
    * Function Function::diff(Symbol const& variable) const
    *
    * Every output differentiated with respect to one variable, which may be a parameter (a
    * sensitivity).
    *---------------------------------------------------------------------------------------------*/
    inline Function Function::diff(Symbol const& variable) const
    {
        std::vector<Expr> outputs;
        outputs.reserve(this->outputs_.size());
        for (Expr const& output : this->outputs_) outputs.push_back(lode::diff(output, variable));
        return Function(this->domain_, Tensor<Expr>(Shape(this->outputs_.shape()), std::move(outputs)),
                        Parameters(this->parameters_));
    }


   /*-----------------------------------------------------------------------------------------------
    * Function Function::grad() const
    *
    * Every output differentiated with respect to every variable of the domain. The result's
    * shape is the outputs' shape followed by the domain's: for outputs F and domain x, the
    * entry (i..., j...) is dF(i...)/dx(j...), and in storage the derivatives of each output are
    * consecutive, in the domain's storage order.
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

        std::vector<std::size_t> shape = this->outputs_.shape();
        shape.insert(shape.end(), this->domain_.shape().begin(), this->domain_.shape().end());
        return Function(this->domain_, Tensor<Expr>(Shape(std::move(shape)), std::move(outputs)),
                        Parameters(this->parameters_));
    }
}
#endif
