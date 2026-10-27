/*--------------------------------------------------------------------------------------------------
 * File: matl_test.cpp
 *
 * Description: Tests of LODE's interoperability with MATL: sparse Jacobians written into MATL
 *              sparse matrices, and matrix-valued functions evaluated into MATL dense matrices.
 *              Built only with -DLODE_WITH_MATL=ON.
 *------------------------------------------------------------------------------------------------*/
#include <cmath>
#include <stdexcept>
#include <vector>

#include "check.hpp"
#include "lode/matl.hpp"

int main()
{
    using lode::Function;
    using lode::Symbol;

    Symbol y1("y1");
    Symbol y2("y2");
    Symbol y3("y3");
    Symbol k1("k1");
    Symbol k2("k2");
    Symbol k3("k3");
    Function rhs({y1, y2, y3},
                 {-k1*y1 + k3*y2*y3, k1*y1 - k3*y2*y3 - k2*pow(y2, 2), k2*pow(y2, 2)},
                 {k1, k2, k3});
    lode::SparseMatrixFunction jacobian = rhs.sparse_jacobian();
    std::vector<double> state{0.9, 2e-5, 0.1};
    std::vector<double> rates{0.04, 3e7, 1e4};

    // The pattern is set up once; each evaluation writes the values in place
    matl::CSRMatrix<double> sparse = lode::csr_matrix(jacobian);
    LODE_CHECK_EQUAL(sparse.rows(), matl::index_t{3});
    LODE_CHECK_EQUAL(sparse.values().size(), std::size_t{7});
    jacobian.evaluate(state, rates, sparse.values());

    // MATL's entries count from 1, like LODE's: compare with the dense Jacobian entry by entry
    std::vector<double> inputs{0.9, 2e-5, 0.1, 0.04, 3e7, 1e4};
    matl::Matrix<double> dense(3, 3);
    lode::evaluate(rhs.grad(), inputs, dense);
    matl::Matrix<double> from_sparse = matl::dense(sparse);
    bool equal = true;
    for (matl::index_t i = 1; i <= 3; ++i)
        for (matl::index_t j = 1; j <= 3; ++j)
        {
            double difference = std::abs(from_sparse(i, j) - dense(i, j));
            equal = equal && difference <= 1e-12 * std::abs(dense(i, j));
        }
    LODE_CHECK(equal);
    LODE_CHECK_EQUAL(dense(3, 2), 6e7 * 2e-5);

    // A sparse matrix-vector product with the Jacobian: J*v, using MATL
    matl::Matrix<double> v = {{1.0}, {2.0}, {3.0}};
    matl::Matrix<double> product = sparse * v;
    LODE_CHECK(std::abs(product(1, 1) - (-0.04 + 2 * 1e4 * 0.1 + 3 * 1e4 * 2e-5)) < 1e-9);

    // A dense matrix of the wrong dimensions is refused
    matl::Matrix<double> wrong(2, 3);
    LODE_CHECK_THROWS(lode::evaluate(rhs.grad(), inputs, wrong), std::invalid_argument);

    return lode::test::report();
}
