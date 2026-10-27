/*--------------------------------------------------------------------------------------------------
 * File: codegen_cases.hpp
 *
 * Description: The functions whose generated C++ code is tested. The generator program and the
 *              test both build them, in the same way, so that the test can compare the compiled
 *              generated code with LODE's interpreter on the same functions.
 *------------------------------------------------------------------------------------------------*/
#ifndef LODE_TESTS_CODEGEN_CASES_HPP
#define LODE_TESTS_CODEGEN_CASES_HPP

#include <string>
#include <vector>

#include "lode/lode.hpp"
#include "random_expressions.hpp"

namespace lode::test
{
    // A function and the name its generated code is given
    struct CodegenCase
    {
        std::string name;
        Function    function;
    };


   /*-----------------------------------------------------------------------------------------------
    * std::vector<CodegenCase> codegen_cases()
    *
    * Builds the test functions: Robertson's kinetics with parameters, edge cases (a constant
    * function, unused inputs, special functions, awkward variable names), and random functions
    * with their Jacobians.
    *---------------------------------------------------------------------------------------------*/
    inline std::vector<CodegenCase> codegen_cases()
    {
        std::vector<CodegenCase> cases;

        // Robertson's kinetics, with the rate constants as parameters
        Symbol y1("y1");
        Symbol y2("y2");
        Symbol y3("y3");
        Symbol k1("k1");
        Symbol k2("k2");
        Symbol k3("k3");
        Function rhs({y1, y2, y3},
                     {-k1*y1 + k3*y2*y3, k1*y1 - k3*y2*y3 - k2*pow(y2, 2), k2*pow(y2, 2)},
                     {k1, k2, k3});
        cases.push_back({"robertson_rhs", rhs});
        cases.push_back({"robertson_jacobian", rhs.grad()});

        // Edge cases
        Symbol x("x");
        Symbol y("y");
        Symbol z("z");
        cases.push_back({"constant", Function({x}, Expr(7))});
        cases.push_back({"ignores_y", Function({x, y}, 2*x)});
        cases.push_back({"special", Function({x, y}, {pow(x, y), pow(Expr(2), x), pow(x, Number(3, 2)),
                                                      pow(x, Number(-5, 2)), pow(x, Number(1, 3)),
                                                      log(x)*cos(y)/sin(x + 1), exp(-x)*1e300})});

        // Variable names that are C++ keywords, look like generated names, repeat, or are not
        // identifiers
        Symbol keyword("double");
        Symbol generated("v0");
        Symbol first_a("a");
        Symbol second_a("a");
        Symbol primed("x'");
        Expr awkward = keyword*generated + first_a - 2*second_a + primed;
        std::vector<Symbol> awkward_domain{keyword, generated, first_a, second_a, primed};
        cases.push_back({"awkward_names", Function(awkward_domain, awkward)});

        // A function of a matrix variable, its gradient (a matrix) and its Hessian (rank 4)
        Tensor<Symbol> f = make_symbols("F", {2, 2});
        Function det(f, f(1, 1)*f(2, 2) - f(1, 2)*f(2, 1) + exp(f(1, 2)));
        cases.push_back({"determinant", det});
        cases.push_back({"determinant_gradient", det.grad()});
        cases.push_back({"determinant_hessian", det.hessian()});

        // Random vector functions and their Jacobians
        RandomExpressions random(20261027, {x, y, z}, true);
        for (int i = 0; i < 60; ++i)
        {
            Function f({x, y, z}, std::vector<Expr>{random.next(3), random.next(3)});
            cases.push_back({"random_" + std::to_string(i), f});
            cases.push_back({"random_" + std::to_string(i) + "_jacobian", f.grad()});
        }
        return cases;
    }


    // A sparse matrix function and the name its generated code is given
    struct SparseCodegenCase
    {
        std::string          name;
        SparseMatrixFunction matrix;
    };


   /*-----------------------------------------------------------------------------------------------
    * std::vector<SparseCodegenCase> sparse_codegen_cases()
    *
    * Sparse Jacobians whose generated code is tested: Robertson's kinetics with the rate
    * constants as parameters, and a chain of 40 reactions A_i -> A_(i+1) at rates i*A_i.
    *---------------------------------------------------------------------------------------------*/
    inline std::vector<SparseCodegenCase> sparse_codegen_cases()
    {
        Symbol y1("y1");
        Symbol y2("y2");
        Symbol y3("y3");
        Symbol k1("k1");
        Symbol k2("k2");
        Symbol k3("k3");
        Function rhs({y1, y2, y3},
                     {-k1*y1 + k3*y2*y3, k1*y1 - k3*y2*y3 - k2*pow(y2, 2), k2*pow(y2, 2)},
                     {k1, k2, k3});

        std::size_t const n = 40;
        std::vector<Symbol> a;
        for (std::size_t i = 0; i < n; ++i) a.emplace_back("a" + std::to_string(i));
        std::vector<Expr> rates(n, Expr(0));
        for (std::size_t i = 0; i + 1 < n; ++i)
        {
            Expr rate = static_cast<int>(i + 1)*pow(a[i], 2);
            rates[i]     -= rate;
            rates[i + 1] += rate;
        }

        return {{"robertson_sparse", rhs.sparse_jacobian()},
                {"chain_sparse", Function(a, rates).sparse_jacobian()}};
    }
}
#endif
