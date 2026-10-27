/*--------------------------------------------------------------------------------------------------
 * File: codegen.hpp
 *
 * Description: C++ code generation: writes a Function as plain C++ source, to be compiled into a
 *              program for the fastest evaluation, with no interpreter.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_CODEGEN_HPP
#define LODE_CODEGEN_HPP

#include <algorithm>
#include <bit>
#include <cctype>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#include "lode/function.hpp"
#include "lode/version.hpp"

namespace lode
{
    // The C++ source of a header that defines the function under the given name; see the
    // definition for what the header contains. Throws std::invalid_argument if the name is not
    // a valid C++ identifier.
    std::string generate_cpp(Function const& function, std::string const& name);

    // Writes generate_cpp(function, name) to a file, creating its directory if needed. Throws
    // std::runtime_error if the file cannot be written.
    void write_cpp(Function const& function, std::string const& name,
                   std::filesystem::path const& path);


    namespace detail
    {
        // Words that cannot name a variable in the generated code: C++ keywords, and the names
        // the generated code uses itself
        inline bool is_reserved_name(std::string const& name)
        {
            static std::set<std::string> const RESERVED = {
                "alignas", "alignof", "and", "asm", "auto", "bool", "break", "case", "catch",
                "char", "class", "concept", "const", "consteval", "constexpr", "constinit",
                "continue", "decltype", "default", "delete", "do", "double", "else", "enum",
                "explicit", "export", "extern", "false", "float", "for", "friend", "goto", "if",
                "inline", "int", "long", "mutable", "namespace", "new", "noexcept", "not",
                "nullptr", "operator", "or", "private", "protected", "public", "register",
                "requires", "return", "short", "signed", "sizeof", "static", "struct", "switch",
                "template", "this", "throw", "true", "try", "typedef", "typename", "union",
                "unsigned", "using", "virtual", "void", "volatile", "while", "xor",
                "T", "point", "points", "parameters", "inputs", "outputs", "count", "p", "std",
                "cos", "exp", "log", "pow", "sin", "sqrt"};
            return RESERVED.count(name) > 0;
        }

        // Whether text is a C++ identifier that is safe to use: letters, digits and
        // underscores, not starting with a digit, not reserved for the implementation (a
        // leading underscore and capital, or a double underscore), and not a keyword
        inline bool is_identifier(std::string const& text)
        {
            if (text.empty() || std::isdigit(static_cast<unsigned char>(text.front()))) return false;
            for (char c : text)
                if (!std::isalnum(static_cast<unsigned char>(c)) && c != '_') return false;
            if (text.find("__") != std::string::npos) return false;
            if (text.size() > 1 && text[0] == '_' && std::isupper(static_cast<unsigned char>(text[1])))
                return false;
            return !is_reserved_name(text);
        }

        // Whether a name looks like one the generator makes up (v12, c3, input_4)
        inline bool is_generated_name(std::string const& name)
        {
            auto all_digits = [](std::string const& text)
            {
                auto is_digit = [](char c) {return std::isdigit(static_cast<unsigned char>(c)) != 0;};
                return !text.empty() && std::all_of(text.begin(), text.end(), is_digit);
            };
            if ((name[0] == 'v' || name[0] == 'c') && all_digits(name.substr(1))) return true;
            return name.rfind("input_", 0) == 0 && all_digits(name.substr(6));
        }

        // A C++ literal for a constant, exact when read back
        inline std::string cpp_literal(double value)
        {
            // Built from double and converted, as the interpreter does: std::numeric_limits<T> is
            // not defined for class types such as std::complex, where it would give 0
            if (std::isnan(value)) return "static_cast<T>(std::numeric_limits<double>::quiet_NaN())";
            if (std::isinf(value))
            {
                return value > 0 ? "static_cast<T>(std::numeric_limits<double>::infinity())"
                                 : "static_cast<T>(-std::numeric_limits<double>::infinity())";
            }
            char buffer[32];
            std::to_chars_result result = std::to_chars(buffer, buffer + sizeof(buffer), value);
            std::string text(buffer, result.ptr);
            if (text.find_first_of(".e") == std::string::npos) text += ".0";
            return "static_cast<T>(" + text + ")";
        }


       /*-------------------------------------------------------------------------------------------
        * class CppWriter
        *
        * Writes a lowered program as the body of a C++ function. Each value becomes a const local
        * variable, assigned once; the C++ compiler keeps them in machine registers, so the
        * generated code needs no register assignment of its own. Inputs are named after their
        * variables where possible, constants are named once at the top, and computed values are
        * numbered in order: v0, v1, ...
        *-----------------------------------------------------------------------------------------*/
        class CppWriter
        {
          private:
            std::vector<std::string>             names_;     // Name of each value, "" if unnamed yet
            std::map<std::uint64_t, std::string> constants_; // Name of each constant, by its bits
            std::string                          body_;

          public:
            // Lines of the body, given the lowered program and the inputs' variables
            CppWriter(Lowered const& lowered, std::vector<Symbol> const& inputs,
                      std::size_t domain_size);

            std::string const& body() const {return this->body_;}

          private:
            // Name of a constant, declared the first time it is used
            std::string constant(double value, std::string& declarations);

            // The C++ expression an instruction computes
            std::string expression(Instruction const& in, std::string& declarations);
        };


       /*-------------------------------------------------------------------------------------------
        * CppWriter::CppWriter(Lowered const& lowered, std::vector<Symbol> const& inputs,
        *                      std::size_t domain_size)
        *
        * Writes the body: input loads for the inputs that are read, constant declarations, then
        * one line per instruction.
        *
        * Parameters:
        *     lowered        The program
        *     inputs         The variables of the inputs: the domain, then the parameters
        *     domain_size    How many inputs are domain variables (read from `point`); the rest
        *                    are parameters (read from `parameters`)
        *-----------------------------------------------------------------------------------------*/
        inline CppWriter::CppWriter(Lowered const& lowered, std::vector<Symbol> const& inputs,
                                    std::size_t domain_size)
            : names_(lowered.value_count)
        {
            // Inputs that some instruction reads
            std::vector<bool> read(lowered.input_count, false);
            for (Instruction const& in : lowered.code)
            {
                if (reads_a(in.op) && in.a < lowered.input_count) read[in.a] = true;
                if (reads_b(in.op) && in.b < lowered.input_count) read[in.b] = true;
            }

            // Name inputs after their variables when the names are usable and distinct
            std::set<std::string> taken;
            std::string loads;
            for (std::size_t i = 0; i < lowered.input_count; ++i)
            {
                std::string name = inputs[i].name();
                if (!is_identifier(name) || is_generated_name(name) || taken.count(name) > 0)
                    name = "input_" + std::to_string(i);
                taken.insert(name);
                this->names_[i] = name;
                if (!read[i]) continue;
                std::string source = i < domain_size
                                   ? "point[" + std::to_string(i) + "]"
                                   : "parameters[" + std::to_string(i - domain_size) + "]";
                loads += "    T const " + name + " = " + source + ";\n";
            }

            // One line per instruction; constants are declared as they are first needed
            std::string declarations;
            std::string lines;
            std::size_t next = 0;
            for (Instruction const& in : lowered.code)
            {
                std::string text = this->expression(in, declarations);
                if (in.op == Op::Store || in.op == Op::StoreConst)
                    lines += "    outputs[" + std::to_string(in.b) + "] = " + text + ";\n";
                else
                {
                    std::string name = "v" + std::to_string(next++);
                    this->names_[in.result] = name;
                    lines += "    T const " + name + " = " + text + ";\n";
                }
            }
            this->body_ = loads + declarations + lines;
        }


       /*-------------------------------------------------------------------------------------------
        * std::string CppWriter::constant(double value, std::string& declarations)
        *
        * The name of a constant (c0, c1, ...), adding its declaration the first time.
        *-----------------------------------------------------------------------------------------*/
        inline std::string CppWriter::constant(double value, std::string& declarations)
        {
            std::uint64_t bits = std::bit_cast<std::uint64_t>(value);
            auto it = this->constants_.find(bits);
            if (it != this->constants_.end()) return it->second;

            std::string name = "c" + std::to_string(this->constants_.size());
            this->constants_.emplace(bits, name);
            declarations += "    T const " + name + " = " + cpp_literal(value) + ";\n";
            return name;
        }


       /*-------------------------------------------------------------------------------------------
        * std::string CppWriter::expression(Instruction const& in, std::string& declarations)
        *
        * The C++ expression for an instruction's result (for a store, the value stored).
        *-----------------------------------------------------------------------------------------*/
        inline std::string CppWriter::expression(Instruction const& in, std::string& declarations)
        {
            std::string a = reads_a(in.op) ? this->names_[in.a] : "";
            std::string b = reads_b(in.op) ? this->names_[in.b] : "";
            bool uses_constant = in.op == Op::AddConst || in.op == Op::MulConst
                              || in.op == Op::MulAddConst || in.op == Op::ConstDiv
                              || in.op == Op::PowConst || in.op == Op::ConstPow
                              || in.op == Op::StoreConst;
            std::string c = uses_constant ? this->constant(in.constant, declarations) : "";

            switch (in.op)
            {
                case Op::Add:         return a + " + " + b;
                case Op::Sub:         return a + " - " + b;
                case Op::Mul:         return a + " * " + b;
                case Op::Div:         return a + " / " + b;
                case Op::Neg:         return "-" + a;
                case Op::AddConst:    return a + " + " + c;
                case Op::MulConst:    return a + " * " + c;
                case Op::MulAddConst: return a + " * " + c + " + " + b;
                case Op::ConstDiv:    return c + " / " + a;
                case Op::Sqrt:        return "sqrt(" + a + ")";
                case Op::PowConst:    return "pow(" + a + ", " + c + ")";
                case Op::ConstPow:    return "pow(" + c + ", " + a + ")";
                case Op::Pow:         return "pow(" + a + ", " + b + ")";
                case Op::Exp:         return "exp(" + a + ")";
                case Op::Log:         return "log(" + a + ")";
                case Op::Sin:         return "sin(" + a + ")";
                case Op::Cos:         return "cos(" + a + ")";
                case Op::Store:       return a;
                case Op::StoreConst:  return c;
            }
            return "";
        }


        // A function's declaration line, with its parameters wrapped and aligned after the
        // opening parenthesis if the line would pass column 100
        inline std::string signature(std::string const& head, std::string const& parameters)
        {
            std::string line   = head + "(";
            std::string indent(line.size(), ' ');
            std::string text;
            std::size_t start = 0;
            while (start < parameters.size())
            {
                std::size_t comma  = parameters.find(", ", start);
                bool        last   = comma == std::string::npos;
                std::size_t length = last ? std::string::npos : comma - start;
                std::string piece  = parameters.substr(start, length);
                std::string item = piece + (last ? ")" : ",");
                if (line.size() + item.size() > 100 && line.size() > indent.size())
                {
                    line.pop_back();   // the space after the last comma
                    text += line + "\n";
                    line = indent;
                }
                line += item + (last ? "" : " ");
                start = last ? parameters.size() : comma + 2;
            }
            return text + line;
        }

        // "y1, y2, y3": names of a list of variables, for comments
        inline std::string name_list(std::vector<Symbol> const& variables)
        {
            std::string text;
            for (std::size_t i = 0; i < variables.size(); ++i)
                text += (i == 0 ? "" : ", ") + variables[i].name();
            return text;
        }
    }


   /*-----------------------------------------------------------------------------------------------
    * std::string generate_cpp(Function const& function, std::string const& name)
    *
    * The C++ source of a self-contained header defining the function. For a function `name` of
    * n variables, m parameters and k outputs it defines, as templates on the number type T:
    *
    *     name(point, parameters, outputs)          one point: n values and m values in, k out
    *     name(inputs, outputs)                     the same, all n + m inputs in one array
    *     name_batch(points, parameters, outputs, count)
    *                                               count points sharing the parameter values:
    *                                               points[p*n + i] in, outputs[p*k + j] out
    *
    * Without parameters there is no `parameters` argument: name(point, outputs) and
    * name_batch(points, outputs, count). Outputs are in row-major order, as in the Function.
    *
    * The body is the same optimized program the interpreter runs (shared subexpressions,
    * power chains, folded constants), written as straight-line code: one `T const` variable per
    * value. The C++ compiler then assigns machine registers, inlines the function into its
    * callers, and schedules the instructions, so no interpreter overhead remains.
    *
    * Parameters:
    *     function    Function to generate
    *     name        Name of the generated function (a C++ identifier)
    *
    * Returns:
    *     The header's text
    *---------------------------------------------------------------------------------------------*/
    inline std::string generate_cpp(Function const& function, std::string const& name)
    {
        if (!detail::is_identifier(name))
            throw std::invalid_argument("generate_cpp: '" + name + "' is not a usable C++ identifier");

        // The same lowering the interpreter uses, before registers are assigned
        detail::Compiler       compiler(function.domain(), function.parameters());
        detail::Lowered const  lowered = compiler.lower_outputs(function.outputs());
        std::vector<Symbol>    inputs  = function.domain();
        inputs.insert(inputs.end(), function.parameters().begin(), function.parameters().end());
        detail::CppWriter writer(lowered, inputs, function.domain().size());

        std::size_t const n_domain    = function.domain().size();
        bool const        parameters  = !function.parameters().empty();
        std::string const n_in        = std::to_string(n_domain);
        std::string const n_out       = std::to_string(function.size());
        std::string       guard       = "LODE_GENERATED_" + name + "_HPP";
        auto upper = [](char c)
        {return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));};
        std::transform(guard.begin(), guard.end(), guard.begin(), upper);

        // What the function is, for the reader: a scalar, a vector, a matrix or an array
        std::vector<std::size_t> const& dimensions = function.shape();
        std::string shape = "a scalar";
        if (dimensions.size() == 1) shape = "a vector";
        if (dimensions.size() >= 2)
        {
            shape = "a ";
            for (std::size_t d = 0; d < dimensions.size(); ++d)
                shape += (d == 0 ? "" : "x") + std::to_string(dimensions[d]);
            shape += dimensions.size() == 2 ? " matrix, row by row" : " array, in row-major order";
        }
        std::string text = function.to_string();
        bool show_text = text.size() <= 70;

        std::string rule_top    = "/*" + std::string(98, '-');
        std::string rule_bottom = " *" + std::string(96, '-') + "*/";
        std::string out;
        out += rule_top + "\n";
        out += " * File: " + name + ".hpp\n";
        out += " *\n";
        out += " * Description: Generated by LODE " + std::string(VERSION) + ". Do not edit;\n";
        out += " *              regenerate it from the Function instead.\n";
        out += " *\n";
        out += " *              Variables:  " + detail::name_list(function.domain()) + "\n";
        if (parameters)
            out += " *              Parameters: " + detail::name_list(function.parameters()) + "\n";
        out += " *              Outputs:    " + n_out + " (" + shape + ")\n";
        if (show_text) out += " *              Function:   " + text + "\n";
        out += rule_bottom + "\n";
        out += "#ifndef " + guard + "\n#define " + guard + "\n\n";
        out += "#include <cmath>\n#include <cstddef>\n#include <limits>\n\n";

        // One point
        std::string point_parameters = parameters
            ? "[[maybe_unused]] T const* point, [[maybe_unused]] T const* parameters, T* outputs"
            : "[[maybe_unused]] T const* point, T* outputs";
        out += "// Evaluates " + name + " at one point\n";
        out += "template<class T>\n";
        out += detail::signature("inline void " + name, point_parameters) + "\n{\n";
        out += "    using std::cos;\n    using std::exp;\n    using std::log;\n";
        out += "    using std::pow;\n    using std::sin;\n    using std::sqrt;\n\n";
        out += writer.body();
        out += "}\n\n";

        // All inputs in one array (only differs when there are parameters)
        if (parameters)
        {
            out += "// The same, with the variables and then the parameters in one array\n";
            out += "template<class T>\n";
            out += "inline void " + name + "(T const* inputs, T* outputs)\n";
            out += "{" + name + "(inputs, inputs + " + n_in + ", outputs);}\n\n";
        }

        // Many points
        out += "// Evaluates count points: points[p*" + n_in + " + i] in, outputs[p*" + n_out
             + " + j] out\n";
        out += "template<class T>\n";
        std::string batch_parameters = parameters
            ? "T const* points, T const* parameters, T* outputs, std::size_t count"
            : "T const* points, T* outputs, std::size_t count";
        std::string call = parameters
            ? name + "(points + p * " + n_in + ", parameters, outputs + p * " + n_out + ");"
            : name + "(points + p * " + n_in + ", outputs + p * " + n_out + ");";
        out += detail::signature("inline void " + name + "_batch", batch_parameters) + "\n";
        out += "{\n    for (std::size_t p = 0; p < count; ++p)\n        " + call + "\n}\n\n";
        out += "#endif\n";
        return out;
    }


   /*-----------------------------------------------------------------------------------------------
    * void write_cpp(Function const& function, std::string const& name,
    *                std::filesystem::path const& path)
    *
    * Writes the generated header to a file, creating its directory if needed.
    *---------------------------------------------------------------------------------------------*/
    inline void write_cpp(Function const& function, std::string const& name,
                          std::filesystem::path const& path)
    {
        std::string source = generate_cpp(function, name);
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path);
        file << source;
        if (!file) throw std::runtime_error("write_cpp: could not write " + path.string());
    }
}
#endif
