// Copyright 2020 Tencent
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#define CATCH_CONFIG_MAIN
#include "turbo_transformers/layers/kernels/activation.h"

#include "turbo_transformers/core/half.h"
#ifdef FT_WITH_CUDA
#include "turbo_transformers/core/cuda_device_context.h"
#endif
#include "catch2/catch.hpp"
#include "loguru.hpp"
#include "turbo_transformers/core/aligned_scratchpad.h"
#include "turbo_transformers/core/enforce.h"
#include "turbo_transformers/layers/kernels/test_helper.h"

namespace turbo_transformers {
namespace layers {
namespace kernels {
template <typename T>
void AddBiasGeluActNaive(const T* bias, T* out, int64_t m, int64_t n) {
  for (int64_t i = 0; i < m; ++i) {
    int64_t k = 0;
    for (int64_t j = n * i; j < n * (i + 1); ++j) {
      auto before_act =
          static_cast<float>(out[j]) + static_cast<float>(bias[k++]);
      out[j] = static_cast<T>(
          before_act * 0.5f *
          (1.0f + std::tanh(0.7978845608028654f *
                            (before_act + 0.044715f * before_act * before_act *
                                              before_act))));
    }
  }
}

template <typename T>
void AddBiasTanhActNaive(const T* bias, T* out, int64_t m, int64_t n) {
  for (int64_t i = 0; i < m; ++i) {
    int64_t k = 0;
    for (int64_t j = n * i; j < n * (i + 1); ++j) {
      auto before_act =
          static_cast<float>(out[j]) + static_cast<float>(bias[k++]);
      out[j] = static_cast<T>(std::tanh(before_act));
    }
  }
}

template <typename Func>
void TestFunction(Func&& func, int step, const std::string& infor,
                  double g_bytes) {
  func();
  test::Timer timer;
  for (int i = 0; i < step; ++i) {
    func();
  }
  auto elapse = timer.ElapseSecond() / step;

  LOG_S(INFO) << infor << " cost:" << elapse << " ms, Bandwidth "
              << g_bytes / elapse << " GB/s";
}

TEST_CASE("activation CPU AddBiasGelu benchmark") {
  auto tensor_create_and_fill_constant =
      [](std::initializer_list<int64_t> shape, float value) {
        turbo_transformers::core::Tensor tensor(nullptr);
        tensor.Reshape<float>(shape, kDLCPU, 0);
        auto* ptr = tensor.mutableData<float>();
        for (int64_t i = 0; i < tensor.numel(); ++i) {
          ptr[i] = value;
        }
        return tensor;
      };

  int64_t hidden_size = 12 * 64;
  const int step = 10;
  for (auto batch_size : {1, 20, 24})
    for (auto seq_length : {8, 16, 32, 48, 64, 128}) {
      auto m = batch_size * seq_length;
      auto n = hidden_size;

      auto bias = tensor_create_and_fill_constant({n}, 0.01f);
      auto out = tensor_create_and_fill_constant({m, n}, 0.02f);
      auto out_parallel = tensor_create_and_fill_constant({m, n}, 0.02f);

      std::cout << "batch_size: " << batch_size
                << " seq_length: " << seq_length;

      TestFunction(
          [&]() {
            AddBiasGeluActNaive<float>(bias.data<float>(),
                                       out_parallel.mutableData<float>(), m, n);
          },
          step, "AddBiasGeluActNaive", m * n * sizeof(float) / 1e9);

      TestFunction(
          [&]() {
            AddBiasAct<ActivationType, ActivationType::Gelu, float>(bias, &out);
          },
          step, "AddBiasGeluAct OMP", m * n * sizeof(float) / 1e9);

      if (!test::CheckResultOfCPU<float>(out, out_parallel)) {
        FT_THROW("AddBiasGelu test failed");
      }
    }
}

TEST_CASE("activation CPU AddBiasTanh benchmark") {
  auto tensor_create_and_fill_constant =
      [](std::initializer_list<int64_t> shape, float value) {
        turbo_transformers::core::Tensor tensor(nullptr);
        tensor.Reshape<float>(shape, kDLCPU, 0);
        auto* ptr = tensor.mutableData<float>();
        for (int64_t i = 0; i < tensor.numel(); ++i) {
          ptr[i] = value;
        }
        return tensor;
      };

  int64_t hidden_size = 12 * 64;
  const int step = 10;
  for (auto batch_size : {1, 20, 24})
    for (auto seq_length : {8, 16, 32, 48, 64, 128}) {
      auto m = batch_size * seq_length;
      auto n = hidden_size;

      auto bias = tensor_create_and_fill_constant({n}, 0.01f);
      auto out = tensor_create_and_fill_constant({m, n}, 0.02f);
      auto out_parallel = tensor_create_and_fill_constant({m, n}, 0.02f);

      std::cout << "batch_size: " << batch_size
                << " seq_length: " << seq_length;

      TestFunction(
          [&]() {
            AddBiasTanhActNaive<float>(bias.data<float>(),
                                       out_parallel.mutableData<float>(), m, n);
          },
          step, "AddBiasTanhActNaive", m * n * sizeof(float) / 1e9);

      TestFunction(
          [&]() {
            AddBiasAct<ActivationType, ActivationType::Tanh, float>(bias, &out);
          },
          step, "AddBiasTanhAct OMP", m * n * sizeof(float) / 1e9);
      if (!test::CheckResultOfCPU<float>(out, out_parallel)) {
        FT_THROW("AddBiasTanh test failed");
      }
    }
}

}  // namespace kernels
}  // namespace layers
}  // namespace turbo_transformers
