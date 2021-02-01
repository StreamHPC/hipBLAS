/* ************************************************************************
 * Copyright 2016-2021 Advanced Micro Devices, Inc.
 *
 * ************************************************************************ */

#include <fstream>
#include <iostream>
#include <stdlib.h>
#include <vector>

#include "testing_common.hpp"

using namespace std;

/* ============================================================================================ */

template <typename T>
hipblasStatus_t testing_tpsv(const Arguments& argus)
{
    bool FORTRAN       = argus.fortran;
    auto hipblasTpsvFn = FORTRAN ? hipblasTpsv<T, true> : hipblasTpsv<T, false>;

    int                N           = argus.N;
    int                incx        = argus.incx;
    char               char_uplo   = argus.uplo_option;
    char               char_diag   = argus.diag_option;
    char               char_transA = argus.transA_option;
    hipblasFillMode_t  uplo        = char2hipblas_fill(char_uplo);
    hipblasDiagType_t  diag        = char2hipblas_diagonal(char_diag);
    hipblasOperation_t transA      = char2hipblas_operation(char_transA);

    int abs_incx = incx < 0 ? -incx : incx;
    int size_A   = N * N;
    int size_AP  = N * (N + 1) / 2;
    int size_x   = abs_incx * N;

    hipblasStatus_t status = HIPBLAS_STATUS_SUCCESS;

    // argument sanity check, quick return if input parameters are invalid before allocating invalid
    // memory
    if(N < 0 || incx == 0)
    {
        return HIPBLAS_STATUS_INVALID_VALUE;
    }

    // Naming: dK is in GPU (device) memory. hK is in CPU (host) memory
    host_vector<T> hA(size_A);
    host_vector<T> hAP(size_AP);
    host_vector<T> AAT(size_A);
    host_vector<T> hb(size_x);
    host_vector<T> hx(size_x);
    host_vector<T> hx_or_b_1(size_x);
    host_vector<T> hx_or_b_2(size_x);
    host_vector<T> cpu_x_or_b(size_x);

    device_vector<T> dAP(size_AP);
    device_vector<T> dx_or_b(size_x);

    double gpu_time_used, cpu_time_used;
    double hipblas_error;
    double hipblasGflops, cblas_gflops, hipblasBandwidth;

    hipblasHandle_t handle;
    hipblasCreate(&handle);

    // Initial Data on CPU
    srand(1);
    hipblas_init<T>(hA, N, N, 1);

    //  calculate AAT = hA * hA ^ T
    cblas_gemm<T>(HIPBLAS_OP_N,
                  HIPBLAS_OP_T,
                  N,
                  N,
                  N,
                  (T)1.0,
                  hA.data(),
                  N,
                  hA.data(),
                  N,
                  (T)0.0,
                  AAT.data(),
                  N);

    //  copy AAT into hA, make hA strictly diagonal dominant, and therefore SPD
    for(int i = 0; i < N; i++)
    {
        T t = 0.0;
        for(int j = 0; j < N; j++)
        {
            hA[i + j * N] = AAT[i + j * N];
            t += abs(AAT[i + j * N]);
        }
        hA[i + i * N] = t;
    }
    //  calculate Cholesky factorization of SPD matrix hA
    cblas_potrf<T>(char_uplo, N, hA.data(), N);

    //  make hA unit diagonal if diag == rocblas_diagonal_unit
    if(char_diag == 'U' || char_diag == 'u')
    {
        if('L' == char_uplo || 'l' == char_uplo)
            for(int i = 0; i < N; i++)
            {
                T diag = hA[i + i * N];
                for(int j = 0; j <= i; j++)
                    hA[i + j * N] = hA[i + j * N] / diag;
            }
        else
            for(int j = 0; j < N; j++)
            {
                T diag = hA[j + j * N];
                for(int i = 0; i <= j; i++)
                    hA[i + j * N] = hA[i + j * N] / diag;
            }
    }

    hipblas_init<T>(hx, 1, N, abs_incx);
    hb = hx;

    // Calculate hb = hA*hx;
    cblas_trmv<T>(uplo, transA, diag, N, hA.data(), N, hb.data(), incx);
    cpu_x_or_b = hb; // cpuXorB <- B
    hx_or_b_1  = hb;
    hx_or_b_2  = hb;

    regular_to_packed(uplo == HIPBLAS_FILL_MODE_UPPER, (T*)hA, (T*)hAP, N);

    // copy data from CPU to device
    hipMemcpy(dAP, hAP.data(), sizeof(T) * size_AP, hipMemcpyHostToDevice);
    hipMemcpy(dx_or_b, hx_or_b_1.data(), sizeof(T) * size_x, hipMemcpyHostToDevice);

    /* =====================================================================
           HIPBLAS
    =================================================================== */
    if(argus.unit_check || argus.norm_check)
    {
        status = hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST);
        if(status != HIPBLAS_STATUS_SUCCESS)
        {
            hipblasDestroy(handle);
            return status;
        }
        status = hipblasTpsvFn(handle, uplo, transA, diag, N, dAP, dx_or_b, incx);
        if(status != HIPBLAS_STATUS_SUCCESS)
        {
            hipblasDestroy(handle);
            return status;
        }

        // copy output from device to CPU
        hipMemcpy(hx_or_b_1.data(), dx_or_b, sizeof(T) * size_x, hipMemcpyDeviceToHost);

        // Calculating error
        hipblas_error = std::abs(vector_norm_1<T>(N, abs_incx, hx.data(), hx_or_b_1.data()));

        if(argus.unit_check)
        {
            double tolerance = std::numeric_limits<real_t<T>>::epsilon() * 40 * N;
            unit_check_error(hipblas_error, tolerance);
        }
    }

    if(argus.timing)
    {
        hipStream_t stream;
        status = hipblasGetStream(handle, &stream);
        if(status != HIPBLAS_STATUS_SUCCESS)
        {
            hipblasDestroy(handle);
            return status;
        }
        status = hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST);
        if(status != HIPBLAS_STATUS_SUCCESS)
        {
            hipblasDestroy(handle);
            return status;
        }
        int runs = argus.cold_iters + argus.iters;
        for(int iter = 0; iter < runs; iter++)
        {
            if(iter == argus.cold_iters)
                gpu_time_used = get_time_us_sync(stream);

            status = hipblasTpsvFn(handle, uplo, transA, diag, N, dAP, dx_or_b, incx);

            if(status != HIPBLAS_STATUS_SUCCESS)
            {
                hipblasDestroy(handle);
                return status;
            }
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used; // in microseconds

        ArgumentModel<e_uplo_option, e_transA_option, e_diag_option, e_N, e_incx>{}.log_args<T>(
            std::cout,
            argus,
            gpu_time_used,
            tpsv_gflop_count<T>(N),
            tpsv_gbyte_count<T>(N),
            hipblas_error);
    }

    hipblasDestroy(handle);
    return HIPBLAS_STATUS_SUCCESS;
}
