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

#include <stdio.h>
#include <stdlib.h>
#include <vector>

#include "testing_common.hpp"

using hipblasIamaxIaminStridedBatchedModel
    = ArgumentModel<e_a_type, e_N, e_incx, e_stride_scale, e_batch_count>;

template <typename T>
using hipblas_iamax_iamin_strided_batched_t = hipblasStatus_t (*)(hipblasHandle_t handle,
                                                                  int             n,
                                                                  const T*        x,
                                                                  int             incx,
                                                                  hipblasStride   stridex,
                                                                  int             batch_count,
                                                                  int*            result);

template <typename T>
void testing_iamax_iamin_strided_batched_bad_arg(const Arguments&                         arg,
                                                 hipblas_iamax_iamin_strided_batched_t<T> func)
{
    hipblasLocalHandle handle(arg);

    int64_t       N           = 100;
    int64_t       incx        = 1;
    int64_t       batch_count = 2;
    hipblasStride stride_x    = N * incx;

    device_vector<T>   dx(stride_x * batch_count);
    device_vector<int> d_res(batch_count);

    for(auto pointer_mode : {HIPBLAS_POINTER_MODE_HOST, HIPBLAS_POINTER_MODE_DEVICE})
    {

        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, pointer_mode));

        // None of these test cases will write to result so using device pointer is fine for both modes
        EXPECT_HIPBLAS_STATUS(func(nullptr, N, dx, incx, stride_x, batch_count, d_res),
                              HIPBLAS_STATUS_NOT_INITIALIZED);
        EXPECT_HIPBLAS_STATUS(func(handle, N, nullptr, incx, stride_x, batch_count, d_res),
                              HIPBLAS_STATUS_INVALID_VALUE);
        EXPECT_HIPBLAS_STATUS(func(handle, N, dx, incx, stride_x, batch_count, nullptr),
                              HIPBLAS_STATUS_INVALID_VALUE);
    }
}

template <typename T>
void testing_iamax_strided_batched_bad_arg(const Arguments& arg)
{
    bool FORTRAN = arg.api == hipblas_client_api::FORTRAN;
    auto hipblasIamaxStridedBatchedFn
        = FORTRAN ? hipblasIamaxStridedBatched<T, true> : hipblasIamaxStridedBatched<T, false>;

    testing_iamax_iamin_strided_batched_bad_arg<T>(arg, hipblasIamaxStridedBatchedFn);
}

template <typename T>
void testing_iamin_strided_batched_bad_arg(const Arguments& arg)
{
    bool FORTRAN = arg.api == hipblas_client_api::FORTRAN;
    auto hipblasIaminStridedBatchedFn
        = FORTRAN ? hipblasIaminStridedBatched<T, true> : hipblasIaminStridedBatched<T, false>;

    testing_iamax_iamin_strided_batched_bad_arg<T>(arg, hipblasIaminStridedBatchedFn);
}

template <typename T, void REFBLAS_FUNC(int, const T*, int, int*)>
void testing_iamax_iamin_strided_batched(const Arguments&                         arg,
                                         hipblas_iamax_iamin_strided_batched_t<T> func)
{
    int    N            = arg.N;
    int    incx         = arg.incx;
    double stride_scale = arg.stride_scale;
    int    batch_count  = arg.batch_count;

    hipblasStride stridex = size_t(N) * incx * stride_scale;
    size_t        sizeX   = stridex * batch_count;

    hipblasLocalHandle handle(arg);

    // check to prevent undefined memory allocation error
    if(batch_count <= 0 || N <= 0 || incx <= 0)
    {
        // quick return success or invalid value
        device_vector<int> d_hipblas_result_0(std::max(1, batch_count));
        host_vector<int>   h_hipblas_result_0(std::max(1, batch_count));
        hipblas_init_nan(h_hipblas_result_0.data(), std::max(1, batch_count));
        CHECK_HIP_ERROR(hipMemcpy(d_hipblas_result_0,
                                  h_hipblas_result_0,
                                  sizeof(int) * std::max(1, batch_count),
                                  hipMemcpyHostToDevice));

        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));
        CHECK_HIPBLAS_ERROR(
            func(handle, N, nullptr, incx, stridex, batch_count, d_hipblas_result_0));

        if(batch_count > 0)
        {
            host_vector<int> cpu_0(batch_count);
            host_vector<int> gpu_0(batch_count);
            CHECK_HIP_ERROR(hipMemcpy(
                gpu_0, d_hipblas_result_0, sizeof(int) * batch_count, hipMemcpyDeviceToHost));
            unit_check_general<int>(1, batch_count, 1, cpu_0, gpu_0);
        }

        return;
    }

    // Naming: dX is in GPU (device) memory. hK is in CPU (host) memory, plz follow this
    // practice
    host_vector<T>     hx(sizeX);
    device_vector<T>   dx(sizeX);
    host_vector<int>   cpu_result(batch_count);
    host_vector<int>   hipblas_result_host(batch_count);
    host_vector<int>   hipblas_result_device(batch_count);
    device_vector<int> d_hipblas_result(batch_count);

    // Initial Data on CPU
    hipblas_init_vector(
        hx, arg, N, incx, stridex, batch_count, hipblas_client_alpha_sets_nan, true);

    // copy data from CPU to device, does not work for incx != 1
    CHECK_HIP_ERROR(hipMemcpy(dx, hx.data(), sizeof(T) * sizeX, hipMemcpyHostToDevice));

    double gpu_time_used;
    int    hipblas_error_host = 0, hipblas_error_device = 0;

    if(arg.unit_check || arg.norm_check)
    {
        /* =====================================================================
                    HIPBLAS
        =================================================================== */
        // device_pointer
        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));
        CHECK_HIPBLAS_ERROR(func(handle, N, dx, incx, stridex, batch_count, d_hipblas_result));

        CHECK_HIP_ERROR(hipMemcpy(hipblas_result_device,
                                  d_hipblas_result,
                                  sizeof(int) * batch_count,
                                  hipMemcpyDeviceToHost));

        // host_pointer
        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_HOST));
        CHECK_HIPBLAS_ERROR(func(handle, N, dx, incx, stridex, batch_count, hipblas_result_host));

        /* =====================================================================
                    CPU BLAS
        =================================================================== */
        for(int b = 0; b < batch_count; b++)
        {
            REFBLAS_FUNC(N, hx.data() + b * stridex, incx, &(cpu_result[b]));
            // change to Fortran 1 based indexing as in BLAS standard, not cblas zero based indexing
            cpu_result[b] += 1;
        }

        if(arg.unit_check)
        {
            unit_check_general<int>(
                1, 1, batch_count, cpu_result.data(), hipblas_result_host.data());
            unit_check_general<int>(
                1, 1, batch_count, cpu_result.data(), hipblas_result_device.data());
        }
        if(arg.norm_check)
        {
            for(int b = 0; b < batch_count; b++)
            {
                hipblas_error_host   = std::max(hipblas_error_host,
                                              hipblas_abs(hipblas_result_host[b] - cpu_result[b]));
                hipblas_error_device = std::max(
                    hipblas_error_device, hipblas_abs(hipblas_result_device[b] - cpu_result[b]));
            }
        }
    } // end of if unit/norm check

    if(arg.timing)
    {
        hipStream_t stream;
        CHECK_HIPBLAS_ERROR(hipblasGetStream(handle, &stream));
        CHECK_HIPBLAS_ERROR(hipblasSetPointerMode(handle, HIPBLAS_POINTER_MODE_DEVICE));

        int runs = arg.cold_iters + arg.iters;
        for(int iter = 0; iter < runs; iter++)
        {
            if(iter == arg.cold_iters)
                gpu_time_used = get_time_us_sync(stream);

            CHECK_HIPBLAS_ERROR(func(handle, N, dx, incx, stridex, batch_count, d_hipblas_result));
        }
        gpu_time_used = get_time_us_sync(stream) - gpu_time_used;

        hipblasIamaxIaminStridedBatchedModel{}.log_args<T>(std::cout,
                                                           arg,
                                                           gpu_time_used,
                                                           iamax_gflop_count<T>(N),
                                                           iamax_gbyte_count<T>(N),
                                                           hipblas_error_host,
                                                           hipblas_error_device);
    }
}

inline void testname_iamax_strided_batched(const Arguments& arg, std::string& name)
{
    hipblasIamaxIaminStridedBatchedModel{}.test_name(arg, name);
}

template <typename T>
void testing_iamax_strided_batched(const Arguments& arg)
{
    bool FORTRAN = arg.api == hipblas_client_api::FORTRAN;
    auto hipblasIamaxStridedBatchedFn
        = FORTRAN ? hipblasIamaxStridedBatched<T, true> : hipblasIamaxStridedBatched<T, false>;

    testing_iamax_iamin_strided_batched<T, cblas_iamax<T>>(arg, hipblasIamaxStridedBatchedFn);
}

inline void testname_iamin_strided_batched(const Arguments& arg, std::string& name)
{
    hipblasIamaxIaminStridedBatchedModel{}.test_name(arg, name);
}

template <typename T>
void testing_iamin_strided_batched(const Arguments& arg)
{
    bool FORTRAN = arg.api == hipblas_client_api::FORTRAN;
    auto hipblasIaminStridedBatchedFn
        = FORTRAN ? hipblasIaminStridedBatched<T, true> : hipblasIaminStridedBatched<T, false>;

    testing_iamax_iamin_strided_batched<T, cblas_iamin<T>>(arg, hipblasIaminStridedBatchedFn);
}
