/*--------------------------------------------------------------------------------------------------
 * File: program.hpp
 *
 * Description: Compiled programs: a flat list of instructions on numbered registers, and the
 *              loop that runs it for one point or a batch of points. Included by function.hpp;
 *              not meant to be included on its own.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_DETAIL_PROGRAM_HPP
#define LODE_DETAIL_PROGRAM_HPP

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace lode::detail
{
    // Operations of a compiled program. r is the result register, a and b are operand
    // registers, and c is the instruction's constant.
    enum class Op : std::uint8_t
    {
        Add,         // r = a + b
        Sub,         // r = a - b
        Mul,         // r = a*b
        Div,         // r = a/b
        Neg,         // r = -a
        AddConst,    // r = a + c
        MulConst,    // r = a*c
        MulAddConst, // r = a*c + b
        ConstDiv,    // r = c/a
        Sqrt,        // r = sqrt(a)
        PowConst,    // r = a^c
        ConstPow,    // r = c^a
        Pow,         // r = a^b
        Exp,         // r = exp(a)
        Log,         // r = log(a)
        Sin,         // r = sin(a)
        Cos,         // r = cos(a)
        Store,       // output b = a
        StoreConst   // output b = c
    };

    // Which operand registers an operation reads
    inline bool reads_a(Op op) {return op != Op::StoreConst;}
    inline bool reads_b(Op op)
    {return op == Op::Add || op == Op::Sub || op == Op::Mul || op == Op::Div || op == Op::Pow
         || op == Op::MulAddConst;}

    // Whether an operation computes a result register (rather than storing an output)
    inline bool has_result(Op op) {return op != Op::Store && op != Op::StoreConst;}

    // One instruction. For Store and StoreConst, b is the output index and result is unused.
    struct Instruction
    {
        Op            op;
        std::uint32_t result;
        std::uint32_t a;
        std::uint32_t b;
        double        constant;
    };


   /*-----------------------------------------------------------------------------------------------
    * class Program
    *
    * A compiled program. Inputs are loaded into registers 0 to input_count - 1, the instructions
    * run in order, and Store instructions write the outputs. Registers hold one value per point,
    * so the same instructions evaluate one point or a block of points: in a block, each
    * instruction loops over the points, which spreads the cost of dispatching the instruction
    * across the block and lets the compiler vectorize the loop.
    *
    * A program is immutable and can be run from several threads at once; each thread uses its own
    * scratch registers.
    *---------------------------------------------------------------------------------------------*/
    class Program
    {
      public:
        // Points evaluated together in a batch
        static constexpr std::size_t BLOCK = 32;

      private:
        std::size_t              input_count_;
        std::size_t              output_count_;
        std::size_t              register_count_;
        std::vector<Instruction> instructions_;

      public:
        Program(std::size_t input_count, std::size_t output_count, std::size_t register_count,
                std::vector<Instruction> instructions)
            : input_count_(input_count), output_count_(output_count),
              register_count_(register_count), instructions_(std::move(instructions)) {}

        std::size_t input_count() const {return this->input_count_;}
        std::size_t output_count() const {return this->output_count_;}
        std::size_t register_count() const {return this->register_count_;}
        std::vector<Instruction> const& instructions() const {return this->instructions_;}

        // Evaluates one point: input_count inputs, output_count outputs
        template<class T>
        void run(T const* inputs, T* outputs) const
        {this->run(inputs, this->input_count_, static_cast<T const*>(nullptr), outputs);}

        // Evaluates one point whose inputs come in two parts: the first `first_count` from
        // `first`, the rest from `rest`
        template<class T>
        void run(T const* first, std::size_t first_count, T const* rest, T* outputs) const;

        // Evaluates count points. Each point supplies its first `point_size` inputs (stored point
        // by point: points[p*point_size + i]); the remaining inputs, the same for every point,
        // come from `shared`.
        template<class T>
        void run_batch(T const* points, std::size_t point_size, T const* shared, T* outputs,
                       std::size_t count) const;

        // The instructions as text, one per line, for inspection
        std::string listing() const;

      private:
        // Runs the instructions on registers holding `lanes` points each
        template<class T, class LANES>
        void execute(T* registers, LANES lanes, T* outputs) const;
    };


   /*-----------------------------------------------------------------------------------------------
    * void Program::run(T const* first, std::size_t first_count, T const* rest, T* outputs) const
    *
    * Evaluates the program at one point.
    *
    * Template Parameters:
    *     T              Number type
    *
    * Parameters:
    *     first          The first first_count inputs
    *     first_count    How many inputs come from first
    *     rest           The other input_count - first_count inputs
    *     outputs        output_count values, written
    *---------------------------------------------------------------------------------------------*/
    template<class T>
    void Program::run(T const* first, std::size_t first_count, T const* rest, T* outputs) const
    {
        thread_local std::vector<T> registers;
        if (registers.size() < this->register_count_) registers.resize(this->register_count_);
        std::copy(first, first + first_count, registers.begin());
        std::copy(rest, rest + (this->input_count_ - first_count), registers.begin() + first_count);
        this->execute(registers.data(), std::integral_constant<std::size_t, 1>{}, outputs);
    }


   /*-----------------------------------------------------------------------------------------------
    * void Program::run_batch(T const* points, std::size_t point_size, T const* shared,
    *                         T* outputs, std::size_t count) const
    *
    * Evaluates the program at many points, BLOCK points at a time.
    *
    * Template Parameters:
    *     T             Number type
    *
    * Parameters:
    *     points        count*point_size values, point by point
    *     point_size    Inputs supplied by each point (the first point_size inputs)
    *     shared        The other input_count - point_size inputs, the same for every point
    *     outputs       count*output_count values, point by point, written
    *     count         Number of points
    *---------------------------------------------------------------------------------------------*/
    template<class T>
    void Program::run_batch(T const* points, std::size_t point_size, T const* shared, T* outputs,
                            std::size_t count) const
    {
        thread_local std::vector<T> registers;
        std::size_t const needed = this->register_count_ * BLOCK;
        if (registers.size() < needed) registers.resize(needed);

        for (std::size_t first = 0; first < count; first += BLOCK)
        {
            // Load the block's inputs, register by register: input i of point l at i*lanes + l.
            // Shared inputs are copied to every point.
            std::size_t lanes = std::min(BLOCK, count - first);
            for (std::size_t l = 0; l < lanes; ++l)
                for (std::size_t i = 0; i < point_size; ++i)
                    registers[i * lanes + l] = points[(first + l) * point_size + i];
            for (std::size_t i = point_size; i < this->input_count_; ++i)
                for (std::size_t l = 0; l < lanes; ++l)
                    registers[i * lanes + l] = shared[i - point_size];

            this->execute(registers.data(), lanes, outputs + first * this->output_count_);
        }
    }


   /*-----------------------------------------------------------------------------------------------
    * void Program::execute(T* registers, LANES lanes, T* outputs) const
    *
    * Runs the instructions. Register k of point l is registers[k*lanes + l]; output j of point l
    * is outputs[l*output_count + j]. When LANES is a compile-time 1, the loops over points
    * vanish and this is a plain scalar interpreter.
    *
    * Template Parameters:
    *     T            Number type
    *     LANES        std::size_t, or std::integral_constant<std::size_t, 1> for one point
    *
    * Parameters:
    *     registers    Register file, inputs already loaded
    *     lanes        Number of points
    *     outputs      Outputs, written
    *---------------------------------------------------------------------------------------------*/
    template<class T, class LANES>
    void Program::execute(T* registers, LANES lanes, T* outputs) const
    {
        // The standard functions, found for T by argument-dependent lookup if it is a class
        using std::cos;
        using std::exp;
        using std::log;
        using std::pow;
        using std::sin;
        using std::sqrt;

        std::size_t const n      = lanes;
        std::size_t const stride = this->output_count_;
        for (Instruction const& instruction : this->instructions_)
        {
            T*       r = registers + instruction.result * n;
            T const* a = registers + instruction.a * n;
            T const* b = registers + instruction.b * n;
            T const  c = static_cast<T>(instruction.constant);
            switch (instruction.op)
            {
                case Op::Add:
                    for (std::size_t l = 0; l < n; ++l) r[l] = a[l] + b[l];
                    break;
                case Op::Sub:
                    for (std::size_t l = 0; l < n; ++l) r[l] = a[l] - b[l];
                    break;
                case Op::Mul:
                    for (std::size_t l = 0; l < n; ++l) r[l] = a[l] * b[l];
                    break;
                case Op::Div:
                    for (std::size_t l = 0; l < n; ++l) r[l] = a[l] / b[l];
                    break;
                case Op::Neg:
                    for (std::size_t l = 0; l < n; ++l) r[l] = -a[l];
                    break;
                case Op::AddConst:
                    for (std::size_t l = 0; l < n; ++l) r[l] = a[l] + c;
                    break;
                case Op::MulConst:
                    for (std::size_t l = 0; l < n; ++l) r[l] = a[l] * c;
                    break;
                case Op::MulAddConst:
                    for (std::size_t l = 0; l < n; ++l) r[l] = a[l] * c + b[l];
                    break;
                case Op::ConstDiv:
                    for (std::size_t l = 0; l < n; ++l) r[l] = c / a[l];
                    break;
                case Op::Sqrt:
                    for (std::size_t l = 0; l < n; ++l) r[l] = sqrt(a[l]);
                    break;
                case Op::PowConst:
                    for (std::size_t l = 0; l < n; ++l) r[l] = pow(a[l], c);
                    break;
                case Op::ConstPow:
                    for (std::size_t l = 0; l < n; ++l) r[l] = pow(c, a[l]);
                    break;
                case Op::Pow:
                    for (std::size_t l = 0; l < n; ++l) r[l] = pow(a[l], b[l]);
                    break;
                case Op::Exp:
                    for (std::size_t l = 0; l < n; ++l) r[l] = exp(a[l]);
                    break;
                case Op::Log:
                    for (std::size_t l = 0; l < n; ++l) r[l] = log(a[l]);
                    break;
                case Op::Sin:
                    for (std::size_t l = 0; l < n; ++l) r[l] = sin(a[l]);
                    break;
                case Op::Cos:
                    for (std::size_t l = 0; l < n; ++l) r[l] = cos(a[l]);
                    break;
                case Op::Store:
                    for (std::size_t l = 0; l < n; ++l) outputs[l * stride + instruction.b] = a[l];
                    break;
                case Op::StoreConst:
                    for (std::size_t l = 0; l < n; ++l) outputs[l * stride + instruction.b] = c;
                    break;
            }
        }
    }


   /*-----------------------------------------------------------------------------------------------
    * std::string Program::listing() const
    *
    * The instructions as text, such as "r3 = r0*r1" or "out[0] = r3".
    *---------------------------------------------------------------------------------------------*/
    inline std::string Program::listing() const
    {
        std::string text;
        for (Instruction const& in : this->instructions_)
        {
            std::string r = "r" + std::to_string(in.result);
            std::string a = "r" + std::to_string(in.a);
            std::string b = "r" + std::to_string(in.b);
            std::string c = std::isnan(in.constant) ? "nan" : Number(in.constant).to_string();
            std::string out = "out[" + std::to_string(in.b) + "]";
            switch (in.op)
            {
                case Op::Add:         text += r + " = " + a + " + " + b; break;
                case Op::Sub:         text += r + " = " + a + " - " + b; break;
                case Op::Mul:         text += r + " = " + a + "*" + b; break;
                case Op::Div:         text += r + " = " + a + "/" + b; break;
                case Op::Neg:         text += r + " = -" + a; break;
                case Op::AddConst:    text += r + " = " + a + " + " + c; break;
                case Op::MulConst:    text += r + " = " + a + "*" + c; break;
                case Op::MulAddConst: text += r + " = " + a + "*" + c + " + " + b; break;
                case Op::ConstDiv:    text += r + " = " + c + "/" + a; break;
                case Op::Sqrt:        text += r + " = sqrt(" + a + ")"; break;
                case Op::PowConst:    text += r + " = " + a + "^" + c; break;
                case Op::ConstPow:    text += r + " = " + c + "^" + a; break;
                case Op::Pow:         text += r + " = " + a + "^" + b; break;
                case Op::Exp:         text += r + " = exp(" + a + ")"; break;
                case Op::Log:         text += r + " = log(" + a + ")"; break;
                case Op::Sin:         text += r + " = sin(" + a + ")"; break;
                case Op::Cos:         text += r + " = cos(" + a + ")"; break;
                case Op::Store:       text += out + " = " + a; break;
                case Op::StoreConst:  text += out + " = " + c; break;
            }
            text += "\n";
        }
        return text;
    }
}
#endif
