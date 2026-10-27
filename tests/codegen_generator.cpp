/*--------------------------------------------------------------------------------------------------
 * File: codegen_generator.cpp
 *
 * Description: Writes the generated C++ code for the test functions into one header, followed
 *              by dispatch functions that call them by case number. Run during the build; the
 *              header is then compiled into codegen_test.
 *------------------------------------------------------------------------------------------------*/
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "codegen_cases.hpp"
#include "lode/codegen.hpp"

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: codegen_generator <output header>\n";
        return 1;
    }

    try
    {
        std::vector<lode::test::CodegenCase> cases = lode::test::codegen_cases();
        std::string source;
        for (lode::test::CodegenCase const& c : cases)
            source += lode::generate_cpp(c.function, c.name) + "\n";

        // Test scaffolding: call the generated functions by case number
        source += "template<class T>\n"
                  "void generated_evaluate(std::size_t index, T const* inputs, T* outputs)\n{\n";
        source += "    switch (index)\n    {\n";
        for (std::size_t i = 0; i < cases.size(); ++i)
        {
            source += "        case " + std::to_string(i) + ": " + cases[i].name
                    + "(inputs, outputs); break;\n";
        }
        source += "        default: break;\n    }\n}\n\n";

        source += "template<class T>\n"
                  "void generated_evaluate_batch(std::size_t index, T const* points,\n"
                  "                              [[maybe_unused]] T const* parameters, T* outputs,\n"
                  "                              std::size_t count)\n{\n";
        source += "    switch (index)\n    {\n";
        for (std::size_t i = 0; i < cases.size(); ++i)
        {
            bool with_parameters = !cases[i].function.parameters().empty();
            source += "        case " + std::to_string(i) + ": " + cases[i].name + "_batch(points, "
                    + (with_parameters ? "parameters, " : "") + "outputs, count); break;\n";
        }
        source += "        default: break;\n    }\n}\n";

        std::filesystem::path path(argv[1]);
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path);
        file << source;
        if (!file)
        {
            std::cerr << "codegen_generator: could not write " << path << "\n";
            return 1;
        }
    }
    catch (std::exception const& error)
    {
        std::cerr << "codegen_generator: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
