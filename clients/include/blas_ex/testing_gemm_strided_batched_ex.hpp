/* ************************************************************************
 * Copyright (C) 2016-2023 Advanced Micro Devices, Inc. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * ************************************************************************ */

#include <fstream>
#include <iostream>
#include <limits>
#include <stdlib.h>
#include <typeinfo>
#include <vector>

#include "hipblas_unique_ptr.hpp"
#include "testing_common.hpp"

/* ============================================================================================ */

using hipblasGemmStridedBatchedExModel = ArgumentModel<e_a_type,
                                                       e_c_type,
                                                       e_compute_type,
                                                       e_transA,
                                                       e_transB,
                                                       e_M,
                                                       e_N,
                                                       e_K,
                                                       e_alpha,
                                                       e_lda,
                                                       e_ldb,
                                                       e_beta,
                                                       e_ldc,
                                                       e_stride_scale,
                                                       e_batch_count,
                                                       e_with_flags,
                                                       e_flags>;
// strides not logged

inline void testname_gemm_strided_batched_ex(const Arguments& arg, std::string& name)
{
    hipblasGemmStridedBatchedExModel{}.test_name(arg, name);
}

template <typename Ti, typename To = Ti, typename Tex = To>
void testing_gemm_strided_batched_ex_bad_arg(const Arguments& arg)
{
    // Note: hipblasGemmEx and hipblasGemmExWithFlags are essentially the exact same.
    //       Only testing WithFlags version as it has slightly more functionality.
    bool FORTRAN                       = arg.fortran;
    auto hipblasGemmStridedBatchedExFn = FORTRAN ? hipblasGemmStridedBatchedExWithFlagsFortran
                                                 : hipblasGemmStridedBatchedExWithFlags;

    hipblasLocalHandle handle(arg);

    hipblasDatatype_t    aType           = arg.a_type;
    hipblasDatatype_t    bType           = arg.b_type;
    hipblasDatatype_t    cType           = arg.c_type;
    hipblasDatatype_t    computeType     = arg.compute_type;
    hipblasComputeType_t computeTypeGemm = arg.compute_type_gemm;
    hipblasGemmFlags_t   flags           = HIPBLAS_GEMM_FLAGS_NONE;
    hipblasGemmAlgo_t    algo            = HIPBLAS_GEMM_DEFAULT;

    int64_t M           = 101;
    int64_t N           = 100;
    int64_t K           = 102;
    int64_t lda         = 103;
    int64_t ldb         = 104;
    int64_t ldc         = 105;
    int64_t batch_count = 2;

    hipblasOperation_t transA = HIPBLAS_OP_N;
    hipblasOperation_t transB = HIPBLAS_OP_N;

    int64_t colsA = transA == HIPBLAS_OP_N ? N : M;
    int64_t colsB = transB == HIPBLAS_OP_N ? N : M;

    hipblasStride strideA = colsA * lda;
    hipblasStride strideB = colsB * ldb;
    hipblasStride strideC = N * ldc;

    device_vector<Ti> dA(strideA * batch_count);
    device_vector<Ti> dB(strideB * batch_count);
    device_vector<To> dC(strideC * batch_count);

    device_vector<Tex> d_alpha(1), d_beta(1), d_one(1), d_zero(1);
    Tex                h_alpha(1), h_beta(2), h_one(1), h_zero(0);

    if constexpr(std::is_same_v<Tex, hipblasHalf>)
        h_one = float_to_half(1.0f);

    const Tex* alpha = &h_alpha;
    const Tex* beta  = &h_beta;
    const Tex* one   = &h_one;
    const Tex* zero  = &h_zero;

    for(auto pointer_mode : {HIPBLAS_POINTER_MODE_HOST, HIPBLAS_POINTER_MODE_DEVICE})
    {
        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, pointer_mode));

        if(pointer_mode == HIPBLAS_POINTER_MODE_DEVICE)
        {
            CHECK_HIP_ERROR(hipMemcpy(d_alpha, alpha, sizeof(*alpha), hipMemcpyHostToDevice));
            CHECK_HIP_ERROR(hipMemcpy(d_beta, beta, sizeof(*beta), hipMemcpyHostToDevice));
            CHECK_HIP_ERROR(hipMemcpy(d_one, one, sizeof(*one), hipMemcpyHostToDevice));
            CHECK_HIP_ERROR(hipMemcpy(d_zero, zero, sizeof(*zero), hipMemcpyHostToDevice));
            alpha = d_alpha;
            beta  = d_beta;
            one   = d_one;
            zero  = d_zero;
        }

        // clang-format off

        EXPECT_HIPBLAS_STATUS(
            hipblasGemmStridedBatchedExFn(nullptr, transA, transB, M, N, K, alpha,
                           dA, aType, lda, strideA,
                           dB, bType, ldb, strideB, beta,
                           dC, cType, ldc, strideC, batch_count,
#ifdef HIPBLAS_V2
                           computeTypeGemm,
#else
                           computeType,
#endif
                           algo, flags),
            HIPBLAS_STATUS_NOT_INITIALIZED);

        EXPECT_HIPBLAS_STATUS(hipblasGemmStridedBatchedExFn(handle,
                                            (hipblasOperation_t)HIPBLAS_FILL_MODE_FULL,
                                            transB, M, N, K, alpha,
                                            dA, aType, lda, strideA,
                                            dB, bType, ldb, strideB, beta,
                                            dC, cType, ldc, strideC, batch_count,
#ifdef HIPBLAS_V2
                                            computeTypeGemm,
#else
                                            computeType,
#endif
                                            algo, flags),
                              HIPBLAS_STATUS_INVALID_ENUM);
        EXPECT_HIPBLAS_STATUS(hipblasGemmStridedBatchedExFn(handle, transA,
                                            (hipblasOperation_t)HIPBLAS_FILL_MODE_FULL,
                                            M, N, K, alpha,
                                            dA, aType, lda, strideA,
                                            dB, bType, ldb, strideB, beta,
                                            dC, cType, ldc, strideC, batch_count,
#ifdef HIPBLAS_V2
                                            computeTypeGemm,
#else
                                            computeType,
#endif
                                            algo, flags),
                              HIPBLAS_STATUS_INVALID_ENUM);

                EXPECT_HIPBLAS_STATUS(hipblasGemmStridedBatchedExFn(handle, transA, transB, M, N, K, alpha,
                                            dA, aType, lda, strideA,
                                            dB, bType, ldb, strideB, beta,
                                            dC, cType, ldc, strideC, batch_count,
#ifdef HIPBLAS_V2
                                            computeTypeGemm,
#else
                                            computeType,
#endif
                                            (hipblasGemmAlgo_t)HIPBLAS_OP_N,
                                            flags),
                              HIPBLAS_STATUS_INVALID_ENUM);

        if(arg.bad_arg_all)
        {
            EXPECT_HIPBLAS_STATUS(
                hipblasGemmStridedBatchedExFn(
                    handle, transA, transB, M, N, K, alpha,
                    dA, aType, lda, strideA,
                    dB, bType, ldb, strideB, nullptr,
                    dC, cType, ldc, strideC, batch_count,
#ifdef HIPBLAS_V2
                    computeTypeGemm,
#else
                    computeType,
#endif
                    algo, flags),
                HIPBLAS_STATUS_INVALID_VALUE);

            if(pointer_mode == HIPBLAS_POINTER_MODE_HOST)
            {
                // alpha check only for host mode. rocBLAS can handle this in device mode too but shouldn't assume in case this changes.
                EXPECT_HIPBLAS_STATUS(
                    hipblasGemmStridedBatchedExFn(
                        handle, transA, transB, M, N, K, nullptr,
                        dA, aType, lda, strideA,
                        dB, bType, ldb, strideB, beta,
                        dC, cType, ldc, strideC, batch_count,
#ifdef HIPBLAS_V2
                        computeTypeGemm,
#else
                        computeType,
#endif
                        algo, flags),
                    HIPBLAS_STATUS_INVALID_VALUE);

                // again, rocBLAS can handle this in device mode but shouldn't assume
                EXPECT_HIPBLAS_STATUS(hipblasGemmStridedBatchedExFn(handle, transA, transB, M, N, K, alpha,
                                                    nullptr, aType, lda, strideA,
                                                    dB, bType, ldb, strideB, beta,
                                                    dC, cType, ldc, strideC, batch_count,
#ifdef HIPBLAS_V2
                                                    computeTypeGemm,
#else
                                                    computeType,
#endif
                                                    algo, flags),
                                      HIPBLAS_STATUS_INVALID_VALUE);
                EXPECT_HIPBLAS_STATUS(hipblasGemmStridedBatchedExFn(handle, transA, transB, M, N, K, alpha,
                                                    dA, aType, lda, strideA,
                                                    nullptr, bType, ldb, strideB, beta,
                                                    dC, cType, ldc, strideC, batch_count,
#ifdef HIPBLAS_V2
                                                    computeTypeGemm,
#else
                                                    computeType,
#endif
                                                    algo, flags),
                                      HIPBLAS_STATUS_INVALID_VALUE);
                EXPECT_HIPBLAS_STATUS(hipblasGemmStridedBatchedExFn(handle, transA, transB, M, N, K, alpha,
                                                    dA, aType, lda, strideA,
                                                    dB, bType, ldb, strideB, beta,
                                                    nullptr, cType, ldc, strideC, batch_count,
#ifdef HIPBLAS_V2
                                                    computeTypeGemm,
#else
                                                    computeType,
#endif
                                                    algo, flags),
                                      HIPBLAS_STATUS_INVALID_VALUE);
            }

            // If alpha == 0, A and B can be nullptr
            CHECK_HIPBLAS_ERROR(hipblasGemmStridedBatchedExFn(
                handle, transA, transB, M, N, K, zero,
                nullptr, aType, lda, strideA,
                nullptr, bType, ldb, strideB, beta,
                dC, cType, ldc, strideC, batch_count,
#ifdef HIPBLAS_V2
                computeTypeGemm,
#else
                computeType,
#endif
                algo, flags));

            // If K == 0, alpha, A, and B can be nullptr
            CHECK_HIPBLAS_ERROR(hipblasGemmStridedBatchedExFn(handle, transA, transB, M, N, 0, nullptr,
                                              nullptr, aType, lda, strideA,
                                              nullptr, bType, ldb, strideB, beta,
                                              dC, cType, ldc, strideC, batch_count,
#ifdef HIPBLAS_V2
                                              computeTypeGemm,
#else
                                              computeType,
#endif
                                              algo, flags));
        }

        // If M == 0 || N == 0 || batch_count == 0, can have nullptrs
        CHECK_HIPBLAS_ERROR(hipblasGemmStridedBatchedExFn(handle, transA, transB, 0, N, K, nullptr,
                                          nullptr, aType, lda, strideA,
                                          nullptr, bType, ldb, strideB, nullptr,
                                          nullptr, cType, ldc, strideC, batch_count,
#ifdef HIPBLAS_V2
                                          computeTypeGemm,
#else
                                          computeType,
#endif
                                          algo, flags));
        CHECK_HIPBLAS_ERROR(hipblasGemmStridedBatchedExFn(handle, transA, transB, M, 0, K, nullptr,
                                          nullptr, aType, lda, strideA,
                                          nullptr, bType, ldb, strideB, nullptr,
                                          nullptr, cType, ldc, strideC, batch_count,
#ifdef HIPBLAS_V2
                                          computeTypeGemm,
#else
                                          computeType,
#endif
                                          algo, flags));
        CHECK_HIPBLAS_ERROR(hipblasGemmStridedBatchedExFn(handle, transA, transB, M, N, K, nullptr,
                                          nullptr, aType, lda, strideA,
                                          nullptr, bType, ldb, strideB, nullptr,
                                          nullptr, cType, ldc, strideC, 0,
#ifdef HIPBLAS_V2
                                          computeTypeGemm,
#else
                                          computeType,
#endif
                                          algo, flags));

        // clang-format on
    }
}

template <typename Ti, typename To = Ti, typename Tex = To>
void testing_gemm_strided_batched_ex(const Arguments& arg)
{
    bool FORTRAN = arg.fortran;
    auto hipblasGemmStridedBatchedExFn
        = FORTRAN ? hipblasGemmStridedBatchedExFortran : hipblasGemmStridedBatchedExFortran;
    auto hipblasGemmStridedBatchedExWithFlagsFn = FORTRAN
                                                      ? hipblasGemmStridedBatchedExWithFlagsFortran
                                                      : hipblasGemmStridedBatchedExWithFlags;

    hipblasGemmAlgo_t algo = HIPBLAS_GEMM_DEFAULT;

    hipblasOperation_t transA = char2hipblas_operation(arg.transA);
    hipblasOperation_t transB = char2hipblas_operation(arg.transB);
    int                M      = arg.M;
    int                N      = arg.N;
    int                K      = arg.K;
    int                lda    = arg.lda;
    int                ldb    = arg.ldb;
    int                ldc    = arg.ldc;

    hipblasDatatype_t    a_type            = arg.a_type;
    hipblasDatatype_t    b_type            = arg.b_type;
    hipblasDatatype_t    c_type            = arg.c_type;
    hipblasDatatype_t    compute_type      = arg.compute_type;
    hipblasComputeType_t compute_type_gemm = arg.compute_type_gemm;
    hipblasGemmFlags_t   flags             = hipblasGemmFlags_t(arg.flags);

    int batch_count = arg.batch_count;

    int norm_check = arg.norm_check;
    int unit_check = arg.unit_check;
    int timing     = arg.timing;

    Tex h_alpha_Tex = arg.get_alpha<Tex>();
    Tex h_beta_Tex  = arg.get_beta<Tex>();

    int A_row = transA == HIPBLAS_OP_N ? M : K;
    int A_col = transA == HIPBLAS_OP_N ? K : M;
    int B_row = transB == HIPBLAS_OP_N ? K : N;
    int B_col = transB == HIPBLAS_OP_N ? N : K;

    // check here to prevent undefined memory allocation error
    if(M < 0 || N < 0 || K < 0 || lda < A_row || ldb < B_row || ldc < M || batch_count < 0)
    {
        return;
    }

    const size_t stride_A = static_cast<size_t>(lda) * static_cast<size_t>(A_col);
    const size_t stride_B = static_cast<size_t>(ldb) * static_cast<size_t>(B_col);
    const size_t stride_C = static_cast<size_t>(ldc) * static_cast<size_t>(N);

    const size_t size_A = stride_A * batch_count;
    const size_t size_B = stride_B * batch_count;
    const size_t size_C = stride_C * batch_count;

    device_vector<Ti> dA(size_A);
    device_vector<Ti> dB(size_B);
    device_vector<To> dC(size_C);

    device_vector<Tex> d_alpha(1);
    device_vector<Tex> d_beta(1);

    if(!dA || !dB || !dC || !d_alpha || !d_beta)
    {
        PRINT_IF_HIP_ERROR(hipErrorOutOfMemory);
        return;
    }

    double             gpu_time_used, hipblas_error_host, hipblas_error_device;
    hipblasLocalHandle handle(arg);

    // Naming: dX is in GPU (device) memory. hK is in CPU (host) memory
    host_vector<Ti> hA(size_A);
    host_vector<Ti> hB(size_B);
    host_vector<To> hC_host(size_C);
    host_vector<To> hC_device(size_C);
    host_vector<To> hC_gold(size_C);

    // Initial Data on CPU
    hipblas_init_matrix(
        hA, arg, A_row, A_col, lda, stride_A, batch_count, hipblas_client_alpha_sets_nan, true);
    hipblas_init_matrix(
        hB, arg, B_row, B_col, ldb, stride_B, batch_count, hipblas_client_alpha_sets_nan);
    hipblas_init_matrix(
        hC_host, arg, M, N, ldc, stride_C, batch_count, hipblas_client_beta_sets_nan);
    hC_gold = hC_device = hC_host;

    // copy data from CPU to device
    CHECK_HIP_ERROR(hipMemcpy(dA, hA, sizeof(Ti) * size_A, hipMemcpyHostToDevice));
    CHECK_HIP_ERROR(hipMemcpy(dB, hB, sizeof(Ti) * size_B, hipMemcpyHostToDevice));

    CHECK_HIP_ERROR(hipMemcpy(dC, hC_host, sizeof(To) * size_C, hipMemcpyHostToDevice));
    CHECK_HIP_ERROR(hipMemcpy(d_alpha, &h_alpha_Tex, sizeof(Tex), hipMemcpyHostToDevice));
    CHECK_HIP_ERROR(hipMemcpy(d_beta, &h_beta_Tex, sizeof(Tex), hipMemcpyHostToDevice));

    if(unit_check || norm_check)
    {
        // hipBLAS
        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST));
        if(!arg.with_flags)
        {
            CHECK_HIPBLAS_ERROR(hipblasGemmStridedBatchedExFn(handle,
                                                              transA,
                                                              transB,
                                                              M,
                                                              N,
                                                              K,
                                                              &h_alpha_Tex,
                                                              dA,
                                                              a_type,
                                                              lda,
                                                              stride_A,
                                                              dB,
                                                              b_type,
                                                              ldb,
                                                              stride_B,
                                                              &h_beta_Tex,
                                                              dC,
                                                              c_type,
                                                              ldc,
                                                              stride_C,
                                                              batch_count,
#ifdef HIPBLAS_V2
                                                              compute_type_gemm,
#else
                                                              compute_type,
#endif
                                                              algo));
        }
        else
        {
            CHECK_HIPBLAS_ERROR(hipblasGemmStridedBatchedExWithFlagsFn(handle,
                                                                       transA,
                                                                       transB,
                                                                       M,
                                                                       N,
                                                                       K,
                                                                       &h_alpha_Tex,
                                                                       dA,
                                                                       a_type,
                                                                       lda,
                                                                       stride_A,
                                                                       dB,
                                                                       b_type,
                                                                       ldb,
                                                                       stride_B,
                                                                       &h_beta_Tex,
                                                                       dC,
                                                                       c_type,
                                                                       ldc,
                                                                       stride_C,
                                                                       batch_count,
#ifdef HIPBLAS_V2
                                                                       compute_type_gemm,
#else
                                                                       compute_type,
#endif
                                                                       algo,
                                                                       flags));
        }

        CHECK_HIP_ERROR(hipMemcpy(hC_host, dC, sizeof(To) * size_C, hipMemcpyDeviceToHost));
        CHECK_HIP_ERROR(hipMemcpy(dC, hC_device, sizeof(To) * size_C, hipMemcpyHostToDevice));

        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));
        if(!arg.with_flags)
        {
            CHECK_HIPBLAS_ERROR(hipblasGemmStridedBatchedExFn(handle,
                                                              transA,
                                                              transB,
                                                              M,
                                                              N,
                                                              K,
                                                              d_alpha,
                                                              dA,
                                                              a_type,
                                                              lda,
                                                              stride_A,
                                                              dB,
                                                              b_type,
                                                              ldb,
                                                              stride_B,
                                                              d_beta,
                                                              dC,
                                                              c_type,
                                                              ldc,
                                                              stride_C,
                                                              batch_count,
#ifdef HIPBLAS_V2
                                                              compute_type_gemm,
#else
                                                              compute_type,
#endif
                                                              algo));
        }
        else
        {
            CHECK_HIPBLAS_ERROR(hipblasGemmStridedBatchedExWithFlagsFn(handle,
                                                                       transA,
                                                                       transB,
                                                                       M,
                                                                       N,
                                                                       K,
                                                                       d_alpha,
                                                                       dA,
                                                                       a_type,
                                                                       lda,
                                                                       stride_A,
                                                                       dB,
                                                                       b_type,
                                                                       ldb,
                                                                       stride_B,
                                                                       d_beta,
                                                                       dC,
                                                                       c_type,
                                                                       ldc,
                                                                       stride_C,
                                                                       batch_count,
#ifdef HIPBLAS_V2
                                                                       compute_type_gemm,
#else
                                                                       compute_type,
#endif
                                                                       algo,
                                                                       flags));
        }

        CHECK_HIP_ERROR(hipMemcpy(hC_device, dC, sizeof(To) * size_C, hipMemcpyDeviceToHost));

        // CPU BLAS
        for(int b = 0; b < batch_count; b++)
        {
            cblas_gemm<Ti, To, Tex>(transA,
                                    transB,
                                    M,
                                    N,
                                    K,
                                    h_alpha_Tex,
                                    hA.data() + b * stride_A,
                                    lda,
                                    hB.data() + b * stride_B,
                                    ldb,
                                    h_beta_Tex,
                                    hC_gold.data() + b * stride_C,
                                    ldc);
        }

        if(unit_check)
        {
            // check for float16/bfloat16 input
            if((getArchMajor() == 11)
               && ((std::is_same<Tex, float>{} && std::is_same<Ti, hipblasBfloat16>{})
                   || (std::is_same<Tex, float>{} && std::is_same<Ti, hipblasHalf>{})
                   || (std::is_same<Tex, hipblasHalf>{} && std::is_same<Ti, hipblasHalf>{})))
            {
                const double tol = K * sum_error_tolerance_for_gfx11<Tex, Ti, To>;
                near_check_general<To>(M, N, batch_count, ldc, stride_C, hC_gold, hC_host, tol);
                near_check_general<To>(M, N, batch_count, ldc, stride_C, hC_gold, hC_device, tol);
            }
            else
            {
                unit_check_general<To>(M, N, batch_count, ldc, stride_C, hC_gold, hC_host);
                unit_check_general<To>(M, N, batch_count, ldc, stride_C, hC_gold, hC_device);
            }
        }
        if(arg.norm_check)
        {
            hipblas_error_host
                = norm_check_general<To>('F', M, N, ldc, stride_C, hC_gold, hC_host, batch_count);
            hipblas_error_device
                = norm_check_general<To>('F', M, N, ldc, stride_C, hC_gold, hC_device, batch_count);
        }
    }

    if(timing)
    {
        hipStream_t stream;
        CHECK_HIPBLAS_ERROR(hipblasGetStream(handle, &stream));
        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST));

        int runs = arg.cold_iters + arg.iters;
        for(int iter = 0; iter < runs; iter++)
        {
            if(iter == arg.cold_iters)
                gpu_time_used = get_time_us_sync(stream);

            if(!arg.with_flags)
            {
                CHECK_HIPBLAS_ERROR(hipblasGemmStridedBatchedExFn(handle,
                                                                  transA,
                                                                  transB,
                                                                  M,
                                                                  N,
                                                                  K,
                                                                  &h_alpha_Tex,
                                                                  dA,
                                                                  a_type,
                                                                  lda,
                                                                  stride_A,
                                                                  dB,
                                                                  b_type,
                                                                  ldb,
                                                                  stride_B,
                                                                  &h_beta_Tex,
                                                                  dC,
                                                                  c_type,
                                                                  ldc,
                                                                  stride_C,
                                                                  batch_count,
#ifdef HIPBLAS_V2
                                                                  compute_type_gemm,
#else
                                                                  compute_type,
#endif
                                                                  algo));
            }
            else
            {
                CHECK_HIPBLAS_ERROR(hipblasGemmStridedBatchedExWithFlagsFn(handle,
                                                                           transA,
                                                                           transB,
                                                                           M,
                                                                           N,
                                                                           K,
                                                                           &h_alpha_Tex,
                                                                           dA,
                                                                           a_type,
                                                                           lda,
                                                                           stride_A,
                                                                           dB,
                                                                           b_type,
                                                                           ldb,
                                                                           stride_B,
                                                                           &h_beta_Tex,
                                                                           dC,
                                                                           c_type,
                                                                           ldc,
                                                                           stride_C,
                                                                           batch_count,
#ifdef HIPBLAS_V2
                                                                           compute_type_gemm,
#else
                                                                           compute_type,
#endif
                                                                           algo,
                                                                           flags));
            }
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        hipblasGemmStridedBatchedExModel{}.log_args<To>(std::cout,
                                                        arg,
                                                        gpu_time_used,
                                                        gemm_gflop_count<Tex>(M, N, K),
                                                        gemm_gbyte_count<Tex>(M, N, K),
                                                        hipblas_error_host,
                                                        hipblas_error_device);
    }
}
