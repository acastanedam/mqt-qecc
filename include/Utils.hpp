//
// Created by luca on 09/06/22.
//
#ifndef QUNIONFIND_UTILS_HPP
#define QUNIONFIND_UTILS_HPP

#include "TreeNode.hpp"

#include <cassert>
#include <flint/nmod_matxx.h>
#include <iostream>
#include <ostream>
#include <random>
#include <set>
#include <vector>
extern "C" {
#include <flint/nmod_mat.h>
};

class Utils {
public:
    /**
     * Uses flint's integers mod n matrix package nnmod_mat to solve the system given by Mx=b
     * Returns x if there is a solution, or an empty vector if there is no solution
     * By the behaviour of flint's solve function, if there are multiple valid solutions one is returned
     * @param M
     * @param vec
     * @return
     */
    static std::vector<bool> solveSystem(const std::vector<std::vector<bool>>& M, const std::vector<bool>& vec) {
        std::vector<bool> result{};
        long              rows = M.size();
        long              cols = M.at(0).size();
        nmod_mat_t        mat;
        nmod_mat_t        x;
        nmod_mat_t        b;
        mp_limb_t         mod = 2U;
        nmod_mat_init(mat, rows, cols, mod);
        nmod_mat_init(x, cols, 1, mod);
        nmod_mat_init(b, rows, 1, mod);

        for (long i = 0; i < nmod_mat_nrows(mat); i++) {
            for (long j = 0; j < nmod_mat_ncols(mat); j++) {
                nmod_mat_set_entry(mat, i, j, M[i][j]);
            }
        }
        auto bColIdx = nmod_mat_ncols(b) - 1;
        for (long i = 0; i < nmod_mat_nrows(b); i++) {
            mp_limb_t tmp = vec[i];
            nmod_mat_set_entry(b, i, bColIdx, tmp);
        }
        int sol = nmod_mat_can_solve(x, mat, b);
        if (sol == 1) {
            nmod_mat_print_pretty(x);
            std::cout << "solution exists" << std::endl;
            result       = std::vector<bool>(nmod_mat_nrows(x));
            auto xColIdx = nmod_mat_ncols(x) - 1;
            for (long i = 0; i < nmod_mat_nrows(x); i++) {
                result.at(i) = nmod_mat_get_entry(x, i, xColIdx);
            }
        } else {
            std::cout << "no sol" << std::endl;
        }

        return result;
    }

    static std::vector<std::vector<bool>> gauss(const std::vector<std::vector<bool>>& matrix) {
        std::vector<std::vector<bool>> result(matrix.size());
        auto                           mat = getFlintMatrix(matrix);
        mat.set_rref(); // reduced row echelon form
        return getMatrixFromFlint(mat);
    }

    static flint::nmod_matxx getFlintMatrix(const std::vector<std::vector<bool>>& matrix) {
        auto ctxx   = flint::nmodxx_ctx(2);
        auto result = flint::nmod_matxx(matrix.size(), matrix.at(0).size(), 2);
        for (size_t i = 0; i < matrix.size(); i++) {
            for (size_t j = 0; j < matrix.at(0).size(); j++) {
                if (matrix[i][j]) {
                    result.at(i, j) = flint::nmodxx::red(1, ctxx);
                } else {
                    result.at(i, j) = flint::nmodxx::red(0, ctxx);
                }
            }
        }
        return result;
    }

    static std::vector<std::vector<bool>> getMatrixFromFlint(const flint::nmod_matxx& matrix) {
        auto ctxx = flint::nmodxx_ctx(2);

        std::vector<std::vector<bool>> result(matrix.rows());
        auto                           a = flint::nmodxx::red(1, ctxx);

        for (size_t i = 0; i < matrix.rows(); i++) {
            result.at(i) = std::vector<bool>(matrix.cols());
            for (size_t j = 0; j < matrix.cols(); j++) {
                if (matrix.at(i, j) == a) {
                    result[i][j] = 1;
                } else {
                    result[i][j] = 0;
                }
            }
        }
        return result;
    }

    /**
     * Checks if the given vector is in the rowspace of matrix M
     * @param M
     * @param vec
     * @return
     */
    static bool isVectorInRowspace(const std::vector<std::vector<bool>>& M, const std::vector<bool>& vec) {
        return !solveSystem(getTranspose(M), vec).empty();
    }

    /**
     * Computes the transpose of the given matrix
     * @param matrix
     * @return
     */
    static std::vector<std::vector<bool>> getTranspose(const std::vector<std::vector<bool>>& matrix) {
        std::vector<std::vector<bool>> transp(matrix.at(0).size());
        for (auto& i: transp) {
            i = std::vector<bool>(matrix.size());
        }
        for (size_t i = 0; i < matrix.size(); i++) {
            for (size_t j = 0; j < matrix.at(i).size(); j++) {
                transp[j][i] = matrix[i][j];
            }
        }
        return transp;
    }

    /**
     * Standard matrix multiplication
     * @param m1
     * @param m2
     * @return
     */
    static std::vector<std::vector<bool>> rectMatrixMultiply(const std::vector<std::vector<bool>>& m1, const std::vector<std::vector<bool>>& m2) {
        auto mat1   = getFlintMatrix(m1);
        auto mat2   = getFlintMatrix(m2);
        auto result = flint::nmod_matxx(mat1.rows(), mat2.cols(), 2);
        result      = mat1.mul_classical(mat2);
        return getMatrixFromFlint(result);
    }

    static void swapRows(std::vector<std::vector<bool>>& matrix, const std::size_t row1, const std::size_t row2) {
        for (std::size_t col = 0; col < matrix.at(0).size(); col++) {
            std::swap(matrix.at(row1).at(col), matrix.at(row2).at(col));
        }
    }

    static void printGF2matrix(const std::vector<std::vector<bool>>& matrix) {
        for (const auto& i: matrix) {
            for (bool j: i) {
                std::cout << j << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }

    static void printGF2vector(const std::vector<bool>& vector) {
        if (vector.empty()) {
            std::cout << "[]";
            return;
        }
        for (const auto& i: vector) {
            std::cout << i << " ";
        }
        std::cout << std::endl;
    }

    /**
     * Returns a bitstring representing am n-qubit Pauli error (all Z or all X)
     * The qubits have iid error probabilities given by the parameter
     * @param n
     * @param physicalErrRate
     * @return
     */
    static std::vector<bool> sampleErrorIidPauliNoise(const std::size_t n, const double physicalErrRate) {
        std::random_device rd;
        std::mt19937       gen(rd());
        std::vector<bool>  result;

        // Setup the weights, iid noise for each bit
        std::discrete_distribution<> d({1 - physicalErrRate, physicalErrRate});
        for (std::size_t i = 0; i < n; i++) {
            result.emplace_back(d(gen));
        }
        return result;
    }

    /**
         *
         * @param error bool vector representing error
         * @param residual estimate vector that contains residual error at end of function
    */
    static void computeResidualErr(const std::vector<bool>& error, std::vector<bool>& residual) {
        for (std::size_t j = 0; j < residual.size(); j++) {
            residual.at(j) = residual.at(j) ^ error.at(j);
        }
    }
};
#endif //QUNIONFIND_UTILS_HPP
