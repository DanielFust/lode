/*--------------------------------------------------------------------------------------------------
 * File: generate_benchmark_code.cpp
 *
 * Description: Writes the generated C++ code for the benchmark's functions into one header. Run
 *              during the build; the benchmark then compiles it.
 *------------------------------------------------------------------------------------------------*/
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>

#include "benchmark_functions.hpp"
#include "lode/codegen.hpp"

int main(int argc, char** argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: generate_benchmark_code <output header>\n";
        return 1;
    }
    try
    {
        std::string source;
        for (lode::bench::BenchmarkFunction const& b : lode::bench::benchmark_functions())
            source += lode::generate_cpp(b.function, b.name) + "\n";

        std::filesystem::path path(argv[1]);
        if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path());
        std::ofstream file(path);
        file << source;
        if (!file)
        {
            std::cerr << "generate_benchmark_code: could not write " << path << "\n";
            return 1;
        }
    }
    catch (std::exception const& error)
    {
        std::cerr << "generate_benchmark_code: " << error.what() << "\n";
        return 1;
    }
    return 0;
}
