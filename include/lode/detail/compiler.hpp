/*--------------------------------------------------------------------------------------------------
 * File: compiler.hpp
 *
 * Description: Compiles expressions into a Program. Included by function.hpp; not meant to be
 *              included on its own.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_DETAIL_COMPILER_HPP
#define LODE_DETAIL_COMPILER_HPP

#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace lode::detail
{
    // A value during compilation: a constant known now, or a value computed at run time
    struct Operand
    {
        bool          is_constant;
        double        constant;
        std::uint32_t value;      // Index of the computed value (before register allocation)
    };

    // A lowered program: instructions in static single assignment form, before registers are
    // assigned. Values 0 to input_count - 1 are the inputs; every other value is the result of
    // exactly one instruction, which comes before any instruction that reads it.
    struct Lowered
    {
        std::size_t              input_count;
        std::size_t              output_count;
        std::uint32_t            value_count;
        std::vector<Instruction> code;
    };

    // Assigns registers to a lowered program
    std::shared_ptr<Program const> allocate_registers(Lowered const& lowered);

    // Hash and equality of expressions, for maps keyed by structure rather than by node
    struct ExprHash
    {
        std::size_t operator()(Expr const& expr) const {return expr.hash();}
    };


   /*-----------------------------------------------------------------------------------------------
    * class Compiler
    *
    * Compiles output expressions over a domain of variables into a Program.
    *
    * Compilation has two passes. The first lowers the expressions to instructions in which every
    * computed value gets a new index (static single assignment). Along the way:
    *
    *     - equal subexpressions are computed once, even if they are different nodes;
    *     - identical instructions are emitted once (value numbering);
    *     - integer powers are built by repeated squaring from shared smaller powers, so x^2 is
    *       reused for x^3 and x^4;
    *     - operations on constants are done now (constant folding), and constants become part
    *       of instructions (x + 2.5 is one AddConst) instead of occupying registers;
    *     - a product's negative powers are multiplied together and divided once.
    *
    * The second pass assigns registers: a register is reused as soon as the value it holds has
    * been read for the last time, which keeps the register file small and in cache.
    *---------------------------------------------------------------------------------------------*/
    class Compiler
    {
      private:
        // Key for value numbering: an instruction's operation, operands and constant
        struct InstructionKey
        {
            Op            op;
            std::uint32_t a;
            std::uint32_t b;
            std::uint64_t constant_bits;

            bool operator==(InstructionKey const&) const = default;
        };

        struct InstructionKeyHash
        {
            std::size_t operator()(InstructionKey const& key) const
            {
                std::size_t seed = static_cast<std::size_t>(key.op);
                hash_combine(seed, key.a);
                hash_combine(seed, key.b);
                hash_combine(seed, static_cast<std::size_t>(key.constant_bits));
                return seed;
            }
        };

        std::size_t                                                    input_count_;
        std::uint32_t                                                  value_count_;
        std::vector<Instruction>                                       code_;
        std::unordered_map<std::uint64_t, std::uint32_t>               inputs_;  // By variable id
        std::unordered_map<Expr, Operand, ExprHash>                    lowered_; // By expression
        std::unordered_map<InstructionKey, std::uint32_t, InstructionKeyHash> emitted_;
        std::map<std::pair<std::uint32_t, std::uint64_t>, std::uint32_t> powers_; // (base, n)
        std::string                                                    domain_text_;

      public:
        // A compiler whose inputs are the domain's variables, then the parameters; throws
        // std::invalid_argument if a variable appears twice among them
        Compiler(std::vector<Symbol> const& domain, std::vector<Symbol> const& parameters);

        // Lowers the outputs to instructions, without unused ones; throws std::invalid_argument
        // for a variable that is not an input. A compiler lowers one set of outputs.
        Lowered lower_outputs(std::vector<Expr> const& outputs);

        // Lowers the outputs and assigns registers
        std::shared_ptr<Program const> compile(std::vector<Expr> const& outputs)
        {return allocate_registers(this->lower_outputs(outputs));}

      private:
        static Operand constant(double value) {return Operand{true, value, 0};}
        static Operand computed(std::uint32_t value) {return Operand{false, 0.0, value};}

        // Emits an instruction, or reuses an identical one; returns its value
        std::uint32_t emit(Op op, std::uint32_t a, std::uint32_t b, double c);

        // Instructions with constant folding and the constant-operand forms
        Operand unary(Op op, Operand a);
        Operand add(Operand a, Operand b);
        Operand subtract(Operand a, Operand b);
        Operand multiply(Operand a, Operand b);
        Operand divide(Operand a, Operand b);
        Operand power(Operand base, Operand exponent);
        Operand multiply_add(Operand a, double c, Operand b);
        Operand integer_power(Operand base, std::uint64_t n);

        // Lowers an expression, once per distinct expression
        Operand lower(Expr const& expr);
        Operand lower_new(Expr const& expr);
        Operand lower_product(Number const& coefficient, std::vector<Factor> const& factors);

    };


   /*-----------------------------------------------------------------------------------------------
    * Compiler::Compiler(std::vector<Symbol> const& domain, std::vector<Symbol> const& parameters)
    *
    * Assigns input i to the i-th variable of the domain followed by the parameters; throws
    * std::invalid_argument if a variable appears twice among them.
    *---------------------------------------------------------------------------------------------*/
    inline Compiler::Compiler(std::vector<Symbol> const& domain, std::vector<Symbol> const& parameters)
        : input_count_(domain.size() + parameters.size()),
          value_count_(static_cast<std::uint32_t>(domain.size() + parameters.size()))
    {
        std::vector<Symbol> inputs = domain;
        inputs.insert(inputs.end(), parameters.begin(), parameters.end());

        // How variables are described in error messages: "the domain (x, y) or the parameters (T)"
        auto list = [](std::vector<Symbol> const& variables)
        {
            std::string text;
            for (std::size_t i = 0; i < variables.size(); ++i)
                text += (i == 0 ? "" : ", ") + variables[i].name();
            return text;
        };
        this->domain_text_ = "the domain (" + list(domain) + ")";
        if (!parameters.empty()) this->domain_text_ += " or the parameters (" + list(parameters) + ")";

        for (std::size_t i = 0; i < inputs.size(); ++i)
        {
            if (!this->inputs_.try_emplace(inputs[i].id(), static_cast<std::uint32_t>(i)).second)
            {
                throw std::invalid_argument("Function: '" + inputs[i].name()
                                            + "' appears twice in the domain and parameters");
            }
        }
    }


   /*-----------------------------------------------------------------------------------------------
    * Lowered Compiler::lower_outputs(std::vector<Expr> const& outputs)
    *
    * Lowers each output and stores it, then removes instructions whose results are never read
    * (constant folding can leave some behind). A single backward pass finds them, because in
    * single assignment form every value is defined before it is read.
    *---------------------------------------------------------------------------------------------*/
    inline Lowered Compiler::lower_outputs(std::vector<Expr> const& outputs)
    {
        for (std::size_t j = 0; j < outputs.size(); ++j)
        {
            Operand result = this->lower(outputs[j]);
            std::uint32_t index = static_cast<std::uint32_t>(j);
            if (result.is_constant)
                this->code_.push_back(Instruction{Op::StoreConst, 0, 0, index, result.constant});
            else
                this->code_.push_back(Instruction{Op::Store, 0, result.value, index, 0.0});
        }

        // Keep the stores, and every instruction whose result a kept instruction reads
        std::vector<bool> needed(this->value_count_, false);
        std::vector<bool> keep(this->code_.size(), false);
        for (std::size_t k = this->code_.size(); k-- > 0;)
        {
            Instruction const& in = this->code_[k];
            keep[k] = !has_result(in.op) || needed[in.result];
            if (!keep[k]) continue;
            if (reads_a(in.op)) needed[in.a] = true;
            if (reads_b(in.op)) needed[in.b] = true;
        }

        Lowered lowered{this->input_count_, outputs.size(), this->value_count_, {}};
        for (std::size_t k = 0; k < this->code_.size(); ++k)
            if (keep[k]) lowered.code.push_back(this->code_[k]);
        return lowered;
    }


   /*-----------------------------------------------------------------------------------------------
    * std::uint32_t Compiler::emit(Op op, std::uint32_t a, std::uint32_t b, double c)
    *
    * Appends an instruction computing a new value, unless an identical instruction was already
    * emitted, in which case its value is reused. Operands of commutative operations are put in
    * a fixed order first, so a + b and b + a are recognized as the same.
    *---------------------------------------------------------------------------------------------*/
    inline std::uint32_t Compiler::emit(Op op, std::uint32_t a, std::uint32_t b, double c)
    {
        if ((op == Op::Add || op == Op::Mul) && b < a) std::swap(a, b);
        InstructionKey key{op, a, b, std::bit_cast<std::uint64_t>(c)};
        auto it = this->emitted_.find(key);
        if (it != this->emitted_.end()) return it->second;

        std::uint32_t value = this->value_count_++;
        this->code_.push_back(Instruction{op, value, a, b, c});
        this->emitted_.emplace(key, value);
        return value;
    }


   /*-----------------------------------------------------------------------------------------------
    * Operand Compiler::unary(Op op, Operand a)
    *
    * A one-operand function (Neg, Sqrt, Exp, Log, Sin, Cos), folded if the operand is constant.
    *---------------------------------------------------------------------------------------------*/
    inline Operand Compiler::unary(Op op, Operand a)
    {
        if (a.is_constant)
        {
            switch (op)
            {
                case Op::Neg:  return constant(-a.constant);
                case Op::Sqrt: return constant(std::sqrt(a.constant));
                case Op::Exp:  return constant(std::exp(a.constant));
                case Op::Log:  return constant(std::log(a.constant));
                case Op::Sin:  return constant(std::sin(a.constant));
                case Op::Cos:  return constant(std::cos(a.constant));
                default:       break;
            }
        }
        return computed(this->emit(op, a.value, 0, 0.0));
    }

    // a + b
    inline Operand Compiler::add(Operand a, Operand b)
    {
        if (a.is_constant && b.is_constant) return constant(a.constant + b.constant);
        if (a.is_constant) std::swap(a, b);
        if (b.is_constant)
            return b.constant == 0.0 ? a : computed(this->emit(Op::AddConst, a.value, 0, b.constant));
        return computed(this->emit(Op::Add, a.value, b.value, 0.0));
    }

    // a - b
    inline Operand Compiler::subtract(Operand a, Operand b)
    {
        if (b.is_constant) return this->add(a, constant(-b.constant));
        if (a.is_constant) return this->add(this->unary(Op::Neg, b), a);
        return computed(this->emit(Op::Sub, a.value, b.value, 0.0));
    }

    // a*b
    inline Operand Compiler::multiply(Operand a, Operand b)
    {
        if (a.is_constant && b.is_constant) return constant(a.constant * b.constant);
        if (a.is_constant) std::swap(a, b);
        if (b.is_constant)
        {
            if (b.constant == 1.0) return a;
            if (b.constant == -1.0) return this->unary(Op::Neg, a);
            return computed(this->emit(Op::MulConst, a.value, 0, b.constant));
        }
        return computed(this->emit(Op::Mul, a.value, b.value, 0.0));
    }

    // a/b; division by a constant becomes multiplication by its reciprocal
    inline Operand Compiler::divide(Operand a, Operand b)
    {
        if (a.is_constant && b.is_constant) return constant(a.constant / b.constant);
        if (b.is_constant) return this->multiply(a, constant(1.0 / b.constant));
        if (a.is_constant) return computed(this->emit(Op::ConstDiv, b.value, 0, a.constant));
        return computed(this->emit(Op::Div, a.value, b.value, 0.0));
    }

    // base^exponent with a non-integer or computed exponent
    inline Operand Compiler::power(Operand base, Operand exponent)
    {
        if (base.is_constant && exponent.is_constant)
            return constant(std::pow(base.constant, exponent.constant));
        if (exponent.is_constant)
            return computed(this->emit(Op::PowConst, base.value, 0, exponent.constant));
        if (base.is_constant)
            return computed(this->emit(Op::ConstPow, exponent.value, 0, base.constant));
        return computed(this->emit(Op::Pow, base.value, exponent.value, 0.0));
    }

    // a*c + b
    inline Operand Compiler::multiply_add(Operand a, double c, Operand b)
    {
        if (a.is_constant || b.is_constant || c == 1.0 || c == -1.0)
            return this->add(this->multiply(a, constant(c)), b);
        return computed(this->emit(Op::MulAddConst, a.value, b.value, c));
    }


   /*-----------------------------------------------------------------------------------------------
    * Operand Compiler::integer_power(Operand base, std::uint64_t n)
    *
    * base^n for n >= 1 by repeated squaring, remembering each power of each base so that powers
    * share their work: x^4 = (x^2)^2 and x^5 = x^4*x reuse x^2.
    *---------------------------------------------------------------------------------------------*/
    inline Operand Compiler::integer_power(Operand base, std::uint64_t n)
    {
        if (n == 1) return base;
        if (base.is_constant)
            return constant(power_by_squaring(base.constant, static_cast<std::int64_t>(n)));

        auto it = this->powers_.find({base.value, n});
        if (it != this->powers_.end()) return computed(it->second);

        Operand half   = this->integer_power(base, n / 2);
        Operand result = this->multiply(half, half);
        if (n % 2 == 1) result = this->multiply(result, base);
        this->powers_.emplace(std::pair{base.value, n}, result.value);
        return result;
    }


   /*-----------------------------------------------------------------------------------------------
    * Operand Compiler::lower(Expr const& expr)
    *
    * Lowers an expression to instructions, once per distinct expression.
    *---------------------------------------------------------------------------------------------*/
    inline Operand Compiler::lower(Expr const& expr)
    {
        auto it = this->lowered_.find(expr);
        if (it != this->lowered_.end()) return it->second;
        Operand result = this->lower_new(expr);
        this->lowered_.emplace(expr, result);
        return result;
    }


   /*-----------------------------------------------------------------------------------------------
    * Operand Compiler::lower_new(Expr const& expr)
    *
    * Lowers an expression not lowered before. A sum becomes a chain of additions, using
    * MulAddConst for terms with coefficients; a variable is its input register.
    *---------------------------------------------------------------------------------------------*/
    inline Operand Compiler::lower_new(Expr const& expr)
    {
        switch (expr.kind())
        {
            case Kind::Number:
                return constant(expr.number().to_double());

            case Kind::Symbol:
            {
                auto it = this->inputs_.find(expr.node().symbol().id);
                if (it == this->inputs_.end())
                {
                    throw std::invalid_argument("Function: '" + expr.node().symbol().name
                                                + "' is not in " + this->domain_text_);
                }
                return computed(it->second);
            }

            case Kind::Add:
            {
                // Start from a term with coefficient 1 if there is one, so that x - y is one
                // subtraction rather than a negation and an addition
                AddData const& sum = expr.node().add();
                std::size_t start = 0;
                for (std::size_t i = 0; i < sum.terms.size(); ++i)
                {
                    if (sum.terms[i].coefficient.is_one())
                    {
                        start = i;
                        break;
                    }
                }

                Term const& first = sum.terms[start];
                Operand total = this->multiply(this->lower(first.expr),
                                               constant(first.coefficient.to_double()));
                for (std::size_t i = 0; i < sum.terms.size(); ++i)
                {
                    if (i == start) continue;
                    Operand value       = this->lower(sum.terms[i].expr);
                    double  coefficient = sum.terms[i].coefficient.to_double();
                    if (coefficient == -1.0)
                        total = this->subtract(total, value);
                    else
                        total = this->multiply_add(value, coefficient, total);
                }
                return this->add(total, constant(sum.constant.to_double()));
            }

            case Kind::Mul:
                return this->lower_product(expr.node().mul().coefficient, expr.node().mul().factors);

            case Kind::Pow:
            {
                PowData const& power = expr.node().pow();
                return this->lower_product(Number(1), {Factor{power.base, power.exponent}});
            }

            case Kind::Call:
            {
                CallData const& call = expr.node().call();
                Operand argument = this->lower(call.argument);
                switch (call.function)
                {
                    case Elementary::Exp: return this->unary(Op::Exp, argument);
                    case Elementary::Log: return this->unary(Op::Log, argument);
                    case Elementary::Sin: return this->unary(Op::Sin, argument);
                    case Elementary::Cos: return this->unary(Op::Cos, argument);
                }
                break;
            }
        }
        return constant(0.0);
    }


   /*-----------------------------------------------------------------------------------------------
    * Operand Compiler::lower_product(Number const& coefficient, std::vector<Factor> const& factors)
    *
    * Lowers coefficient*product of factors as numerator/denominator, so that all negative powers
    * cost one division. Integer powers use integer_power; a power p/2 uses the square root. The
    * coefficient is applied last, so that 3*x*y and 5*x*y share the work of x*y.
    *---------------------------------------------------------------------------------------------*/
    inline Operand Compiler::lower_product(Number const& coefficient,
                                           std::vector<Factor> const& factors)
    {
        Operand numerator   = constant(1.0);
        Operand denominator = constant(1.0);
        for (Factor const& factor : factors)
        {
            Operand base = this->lower(factor.base);
            if (!factor.exponent.is_number())
            {
                numerator = this->multiply(numerator, this->power(base, this->lower(factor.exponent)));
                continue;
            }

            Number const& exponent = factor.exponent.number();
            bool by_root = exponent.is_rational() && exponent.denominator() == 2;
            if (exponent.is_integer() || by_root)
            {
                // |p| for exponent p or p/2, with p's magnitude taken safely
                std::int64_t  p = exponent.numerator();
                std::uint64_t magnitude = p < 0 ? 0 - static_cast<std::uint64_t>(p)
                                                : static_cast<std::uint64_t>(p);
                Operand root  = by_root ? this->unary(Op::Sqrt, base) : base;
                Operand value = this->integer_power(root, magnitude);
                if (p < 0)
                    denominator = this->multiply(denominator, value);
                else
                    numerator = this->multiply(numerator, value);
            }
            else
            {
                Operand value = this->power(base, constant(exponent.to_double()));
                numerator = this->multiply(numerator, value);
            }
        }

        Operand scale = constant(coefficient.to_double());
        if (numerator.is_constant) return this->divide(this->multiply(numerator, scale), denominator);
        return this->multiply(this->divide(numerator, denominator), scale);
    }


   /*-----------------------------------------------------------------------------------------------
    * std::shared_ptr<Program const> allocate_registers(Lowered const& lowered)
    *
    * Maps values to registers. Inputs start in registers 0 to input_count - 1. Walking the
    * instructions in order, a register is freed after the last instruction that reads its value,
    * and each new value takes a free register if there is one. A result may share a register
    * with one of its own operands, which is safe because every operation reads its operands
    * before writing its result, point by point.
    *---------------------------------------------------------------------------------------------*/
    inline std::shared_ptr<Program const> allocate_registers(Lowered const& lowered)
    {
        std::size_t const NONE = std::numeric_limits<std::size_t>::max();

        // Last instruction reading each value
        std::vector<std::size_t> last_use(lowered.value_count, NONE);
        for (std::size_t k = 0; k < lowered.code.size(); ++k)
        {
            Instruction const& in = lowered.code[k];
            if (reads_a(in.op)) last_use[in.a] = k;
            if (reads_b(in.op)) last_use[in.b] = k;
        }

        std::vector<std::uint32_t> location(lowered.value_count, 0);
        std::vector<std::uint32_t> free_registers;
        std::uint32_t register_count = static_cast<std::uint32_t>(lowered.input_count);
        for (std::uint32_t i = 0; i < lowered.input_count; ++i)
        {
            location[i] = i;
            if (last_use[i] == NONE) free_registers.push_back(i);
        }

        std::vector<Instruction> program;
        program.reserve(lowered.code.size());
        for (std::size_t k = 0; k < lowered.code.size(); ++k)
        {
            Instruction in = lowered.code[k];
            bool        read_a = reads_a(in.op);
            bool        read_b = reads_b(in.op);
            std::uint32_t value_a = in.a;
            std::uint32_t value_b = in.b;
            if (read_a) in.a = location[value_a];
            if (read_b) in.b = location[value_b];

            // Free operands read for the last time
            if (read_a && last_use[value_a] == k) free_registers.push_back(location[value_a]);
            if (read_b && last_use[value_b] == k && value_b != value_a)
                free_registers.push_back(location[value_b]);

            if (has_result(in.op))
            {
                std::uint32_t value = in.result;
                std::uint32_t target;
                if (free_registers.empty())
                    target = register_count++;
                else
                {
                    target = free_registers.back();
                    free_registers.pop_back();
                }
                location[value] = target;
                in.result = target;
                if (last_use[value] == NONE) free_registers.push_back(target);
            }
            program.push_back(in);
        }
        return std::make_shared<Program const>(lowered.input_count, lowered.output_count,
                                               register_count, std::move(program));
    }
}
#endif
