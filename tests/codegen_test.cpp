/*--------------------------------------------------------------------------------------------------
 * File: codegen_test.cpp
 *
 * Description: Tests of C++ code generation. The generated code for the test functions was
 *              written during the build and is compiled into this program; here it is checked
 *              against LODE's interpreter, in double and complex arithmetic and in batches. The
 *              generated text itself, and the errors of generate_cpp, are checked too.
 *------------------------------------------------------------------------------------------------*/
#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "check.hpp"
#include "codegen_cases.hpp"
#include "generated_cases.hpp"
#include "lode/codegen.hpp"

namespace
{
    using Complex = std::complex<double>;

    // Whether two doubles agree to a relative tolerance, or are both NaN, or are equal infinities
    bool close(double a, double b)
    {
        if (std::isnan(a) || std::isnan(b)) return std::isnan(a) && std::isnan(b);
        if (std::isinf(a) || std::isinf(b)) return a == b;
        return std::abs(a - b) <= 1e-12 * std::max({1.0, std::abs(a), std::abs(b)});
    }

    bool close(Complex a, Complex b) {return close(a.real(), b.real()) && close(a.imag(), b.imag());}

    // Whether two arrays agree entry by entry
    template<class T>
    bool all_close(std::vector<T> const& a, std::vector<T> const& b)
    {
        if (a.size() != b.size()) return false;
        for (std::size_t i = 0; i < a.size(); ++i)
            if (!close(a[i], b[i])) return false;
        return true;
    }
}


int main()
{
    std::vector<lode::test::CodegenCase> cases = lode::test::codegen_cases();
    std::mt19937 engine(7);
    std::uniform_real_distribution<double> uniform(0.5, 2.0);

    for (std::size_t c = 0; c < cases.size(); ++c)
    {
        lode::Function const& f = cases[c].function;
        std::size_t const n_in   = f.input_size();
        std::size_t const n_out  = f.size();
        std::size_t const n_args = f.domain().size();
        std::size_t const n_par  = f.parameters().size();

        // Several points, in double and in complex arithmetic
        bool matches = true;
        for (int trial = 0; trial < 5; ++trial)
        {
            std::vector<double> inputs(n_in);
            for (double& value : inputs) value = uniform(engine);
            std::vector<double> expected(n_out);
            std::vector<double> actual(n_out);
            f.evaluate(inputs, expected);
            generated_evaluate(c, inputs.data(), actual.data());
            matches = matches && all_close(expected, actual);

            std::vector<Complex> complex_inputs(n_in);
            for (std::size_t i = 0; i < n_in; ++i) complex_inputs[i] = Complex(inputs[i], 0.25);
            std::vector<Complex> complex_expected(n_out);
            std::vector<Complex> complex_actual(n_out);
            f.evaluate<Complex>(complex_inputs, complex_expected);
            generated_evaluate(c, complex_inputs.data(), complex_actual.data());
            matches = matches && all_close(complex_expected, complex_actual);
        }

        // A batch of points sharing the parameter values
        std::size_t const POINTS = 37;
        std::vector<double> points(POINTS * n_args);
        std::vector<double> parameters(n_par);
        for (double& value : points) value = uniform(engine);
        for (double& value : parameters) value = uniform(engine);
        std::vector<double> expected(POINTS * n_out);
        std::vector<double> actual(POINTS * n_out);
        f.evaluate_batch(points, parameters, expected);
        generated_evaluate_batch(c, points.data(), parameters.data(), actual.data(), POINTS);
        matches = matches && all_close(expected, actual);

        if (!matches)
            std::cerr << "generated code differs from the interpreter: " << cases[c].name << "\n";
        LODE_CHECK(matches);
    }

    // Sparse Jacobians: the generated pattern is LODE's, and the generated values are the
    // interpreter's
    std::vector<lode::test::SparseCodegenCase> sparse = lode::test::sparse_codegen_cases();
    lode::SparseMatrixFunction const& robertson = sparse[0].matrix;
    LODE_CHECK_EQUAL(robertson_sparse_rows, robertson.rows());
    LODE_CHECK_EQUAL(robertson_sparse_columns, robertson.columns());
    LODE_CHECK(std::vector<std::size_t>(robertson_sparse_row_offsets.begin(),
                                        robertson_sparse_row_offsets.end())
               == robertson.row_offsets());
    LODE_CHECK(std::vector<std::size_t>(robertson_sparse_column_indices.begin(),
                                        robertson_sparse_column_indices.end())
               == robertson.column_indices());
    std::vector<double> state{0.9, 2e-5, 0.1};
    std::vector<double> rates{0.04, 3e7, 1e4};
    std::vector<double> expected_values(robertson.nonzero_count());
    std::vector<double> generated_values(robertson.nonzero_count());
    robertson.evaluate(state, rates, expected_values);
    robertson_sparse(state.data(), rates.data(), generated_values.data());
    LODE_CHECK(all_close(expected_values, generated_values));

    lode::SparseMatrixFunction const& chain = sparse[1].matrix;
    LODE_CHECK_EQUAL(chain_sparse_column_indices.size(), chain.nonzero_count());
    LODE_CHECK(std::vector<std::size_t>(chain_sparse_column_indices.begin(),
                                        chain_sparse_column_indices.end())
               == chain.column_indices());
    std::vector<double> amounts(chain.columns());
    for (std::size_t i = 0; i < amounts.size(); ++i) amounts[i] = 1.0 + 0.01 * static_cast<double>(i);
    std::vector<double> chain_expected(chain.nonzero_count());
    std::vector<double> chain_generated(chain.nonzero_count());
    chain.evaluate(amounts, chain_expected);
    chain_sparse(amounts.data(), chain_generated.data());
    LODE_CHECK(all_close(chain_expected, chain_generated));

    // The generated text
    lode::Symbol x("x");
    lode::Symbol y("y");
    lode::Function f({x, y}, x*y + 2);
    std::string source = lode::generate_cpp(f, "product_plus_two");
    LODE_CHECK(source.find("#ifndef LODE_GENERATED_PRODUCT_PLUS_TWO_HPP") != std::string::npos);
    LODE_CHECK(source.find("inline void product_plus_two([[maybe_unused]] T const* point, T* outputs)")
               != std::string::npos);
    LODE_CHECK(source.find("    T const v0 = x * y;\n") != std::string::npos);
    LODE_CHECK(source.find("    outputs[0] = v1;\n") != std::string::npos);
    LODE_CHECK(source.find("inline void product_plus_two_batch(") != std::string::npos);

    // write_cpp writes the same text to a file
    std::filesystem::path directory = std::filesystem::temp_directory_path() / "lode_codegen_test";
    std::filesystem::path path      = directory / "f.hpp";
    lode::write_cpp(f, "product_plus_two", path);
    std::ifstream file(path);
    std::stringstream written;
    written << file.rdbuf();
    LODE_CHECK(written.str() == source);
    std::filesystem::remove_all(directory);

    // Names that cannot be used
    LODE_CHECK_THROWS(lode::generate_cpp(f, "2fast"), std::invalid_argument);
    LODE_CHECK_THROWS(lode::generate_cpp(f, "double"), std::invalid_argument);
    LODE_CHECK_THROWS(lode::generate_cpp(f, "exp"), std::invalid_argument);
    LODE_CHECK_THROWS(lode::generate_cpp(f, "my-function"), std::invalid_argument);

    return lode::test::report();
}
