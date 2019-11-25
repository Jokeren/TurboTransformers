#pragma once
#include "fast_transformers/core/tensor.h"

namespace fast_transformers {
namespace layers {
namespace kernels {

template <typename T>
void AddBiasGeLUAct(const core::Tensor& bias, core::Tensor* out);

}  // namespace kernels
}  // namespace layers
}  // namespace fast_transformers
