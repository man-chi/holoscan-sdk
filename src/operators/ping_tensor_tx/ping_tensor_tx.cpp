/*
 * SPDX-FileCopyrightText: Copyright (c) 2024-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "holoscan/operators/ping_tensor_tx/ping_tensor_tx.hpp"

#include <cuda_runtime.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include <gxf/std/allocator.hpp>
#include "holoscan/utils/cuda_macros.hpp"

namespace holoscan::ops {

void PingTensorTxOp::initialize() {
  // Set up prerequisite parameters before calling Operator::initialize()
  auto frag = fragment();

  // Find if there is an argument for 'allocator'
  auto has_allocator = std::find_if(
      args().begin(), args().end(), [](const auto& arg) { return (arg.name() == "allocator"); });
  // Create the allocator if there is no argument provided.
  if (has_allocator == args().end()) {
    allocator_ = frag->make_resource<UnboundedAllocator>("allocator");
    add_arg(allocator_.get());
  }
  Operator::initialize();
}

void PingTensorTxOp::setup(OperatorSpec& spec) {
  spec.output<holoscan::TensorMap>("out");

  spec.param(allocator_, "allocator", "Allocator", "Allocator used to allocate tensor output.");
  spec.param(storage_type_,
             "storage_type",
             "memory storage type",
             "nvidia::gxf::MemoryStorageType enum indicating where the memory will be stored",
             std::string("system"));
  spec.param(batch_size_,
             "batch_size",
             "batch size",
             "Size of the batch dimension (default: 0). The tensor shape will be "
             "([batch], rows, [columns], [channels]) where [] around a dimension indicates that "
             "it is only present if the corresponding parameter has a size > 0."
             "If 0, no batch dimension will be present.",
             static_cast<int32_t>(0));
  spec.param(rows_,
             "rows",
             "number of rows",
             "Number of rows (default: 32), must be >= 1.",
             static_cast<int32_t>(32));
  spec.param(columns_,
             "columns",
             "number of columns",
             "Number of columns (default: 64). If 0, no column dimension will be present.",
             static_cast<int32_t>(64));
  spec.param(
      channels_,
      "channels",
      "channels",
      "Number of channels (default: 0). If 0, no channel dimension will be present. (default: 0)",
      static_cast<int32_t>(0));
  spec.param(data_type_,
             "data_type",
             "data type for the tensor elements",
             "must be one of {'int8_t', 'int16_t', 'int32_t', 'int64_t', 'uint8_t', 'uint16_t',"
             "'uint32_t', 'uint64_t', 'float', 'double', 'complex<float>', 'complex<double>'}",
             std::string{"uint8_t"});
  spec.param(tensor_name_,
             "tensor_name",
             "output tensor name",
             "output tensor name (default: tensor)",
             std::string{"tensor"});
  spec.param(cuda_stream_pool_,
             "cuda_stream_pool",
             "CUDA Stream Pool",
             "Instance of gxf::CudaStreamPool.");
  spec.param(async_device_allocation_,
             "async_device_allocation",
             "enable asynchronous device allocations",
             "If True, enables asynchronous device memory allocation. For async allocation to be, "
             "used, cuda_stream_pool must also be set.",
             false);
  spec.param(data_, "data", "data", "Data to be transmitted.", std::vector<uint8_t>{});
}

nvidia::gxf::PrimitiveType PingTensorTxOp::primitive_type(const std::string& data_type) {
  HOLOSCAN_LOG_INFO("PingTensorTxOp data type = {}", data_type);
  if (data_type == "int8_t") {
    return nvidia::gxf::PrimitiveType::kInt8;
  } else if (data_type == "int16_t") {
    return nvidia::gxf::PrimitiveType::kInt16;
  } else if (data_type == "int32_t") {
    return nvidia::gxf::PrimitiveType::kInt32;
  } else if (data_type == "int64_t") {
    return nvidia::gxf::PrimitiveType::kInt64;
  } else if (data_type == "uint8_t") {
    return nvidia::gxf::PrimitiveType::kUnsigned8;
  } else if (data_type == "uint16_t") {
    return nvidia::gxf::PrimitiveType::kUnsigned16;
  } else if (data_type == "uint32_t") {
    return nvidia::gxf::PrimitiveType::kUnsigned32;
  } else if (data_type == "uint64_t") {
    return nvidia::gxf::PrimitiveType::kUnsigned64;
  } else if (data_type == "float") {
    return nvidia::gxf::PrimitiveType::kFloat32;
  } else if (data_type == "double") {
    return nvidia::gxf::PrimitiveType::kFloat64;
  } else if (data_type == "complex<float>") {
    return nvidia::gxf::PrimitiveType::kComplex64;
  } else if (data_type == "complex<double>") {
    return nvidia::gxf::PrimitiveType::kComplex128;
  }
  throw std::runtime_error(std::string("Unrecognized data_type: ") + data_type);
}

void PingTensorTxOp::compute([[maybe_unused]] InputContext& op_input, OutputContext& op_output,
                             ExecutionContext& context) {
  // the type of out_message is TensorMap
  TensorMap out_message;

  auto gxf_context = context.context();

  // get Handle to underlying nvidia::gxf::Allocator from std::shared_ptr<holoscan::Allocator>
  auto allocator =
      nvidia::gxf::Handle<nvidia::gxf::Allocator>::Create(gxf_context, allocator_->gxf_cid());

  auto gxf_tensor = std::make_shared<nvidia::gxf::Tensor>();

  // Define the dimensions for the CUDA memory (64 x 32, uint8).
  int batch_size = batch_size_.get();
  int rows = rows_.get();
  int columns = columns_.get();
  int channels = channels_.get();
  auto dtype = element_type();

  std::vector<int32_t> shape_vec;
  if (batch_size > 0) {
    shape_vec.push_back(batch_size);
  }
  shape_vec.push_back(rows);
  if (columns > 0) {
    shape_vec.push_back(columns);
  }
  if (channels > 0) {
    shape_vec.push_back(channels);
  }
  auto tensor_shape = nvidia::gxf::Shape{shape_vec};

  const uint64_t bytes_per_element = nvidia::gxf::PrimitiveTypeSize(dtype);
  auto strides = nvidia::gxf::ComputeTrivialStrides(tensor_shape, bytes_per_element);
  nvidia::gxf::MemoryStorageType storage_type;
  const std::string storage_name = storage_type_.get();
  HOLOSCAN_LOG_DEBUG("storage_name = {}", storage_name);
  if (storage_name == std::string("device")) {
    storage_type = nvidia::gxf::MemoryStorageType::kDevice;
  } else if (storage_name == std::string("host")) {
    storage_type = nvidia::gxf::MemoryStorageType::kHost;
  } else if (storage_name == std::string("system")) {
    storage_type = nvidia::gxf::MemoryStorageType::kSystem;
  } else if (storage_name == std::string("cuda_managed")) {
    storage_type = nvidia::gxf::MemoryStorageType::kCudaManaged;
  } else {
    throw std::runtime_error(
        fmt::format("Unrecognized storage_device ({}), should be one of ['device', 'host', "
                    "'system', 'cuda_managed']",
                    storage_name));
  }

  bool use_async_allocation =
      (storage_type == nvidia::gxf::MemoryStorageType::kDevice && async_device_allocation_.get());
  if (!use_async_allocation) {
    // allocate a tensor of the specified shape and data type
    auto result = gxf_tensor->reshapeCustom(
        tensor_shape, dtype, bytes_per_element, strides, storage_type, allocator.value());
    if (!result) {
      HOLOSCAN_LOG_ERROR("failed to generate tensor");
    }
  } else {
    // Tensor doesn't currently have an API for async allocation so have to allocate with CUDA
    // and then wrap it using wrapMemory.
    const std::string stream_name = fmt::format("{}_stream", name_);
    auto maybe_stream = context.allocate_cuda_stream(stream_name);
    if (!maybe_stream) {
      throw std::runtime_error(
          fmt::format("Failed to allocate CUDA stream: {}", maybe_stream.error().what()));
    }
    auto cuda_stream = maybe_stream.value();
    op_output.set_cuda_stream(cuda_stream, "out");

    // manually use CUDA APIs for async memory allocation since reshapeCustom is synchronous
    // Create a shared pointer for the CUDA memory with a custom deleter.
    auto pointer = std::shared_ptr<void*>(new void*, [cuda_stream](void** pointer) {
      if (pointer != nullptr) {
        HOLOSCAN_CUDA_CALL_THROW_ERROR(cudaFreeAsync(*pointer, cuda_stream),
                                       "Failed to free CUDA memory");
        delete pointer;
      }
    });

    // Allocate CUDA device memory (for this test operator, the values are uninitialized)
    size_t nbytes = tensor_shape.size() * bytes_per_element;
    HOLOSCAN_CUDA_CALL_THROW_ERROR(cudaMallocAsync(pointer.get(), nbytes, cuda_stream),
                                   "Failed to allocate CUDA memory");

    // wrap this memory as the GXF tensor
    gxf_tensor->wrapMemory(tensor_shape,
                           dtype,
                           bytes_per_element,
                           strides,
                           storage_type,
                           *pointer,
                           [orig_pointer = pointer](void*) mutable {
                             orig_pointer.reset();  // decrement ref count
                             return nvidia::gxf::Success;
                           });
  }

  if (data_.get().size() > 0) {
    // copy the data into the tensor
    if (storage_type == nvidia::gxf::MemoryStorageType::kDevice) {
      HOLOSCAN_CUDA_CALL_THROW_ERROR(cudaMemcpy(gxf_tensor->pointer(),
                                                data_.get().data(),
                                                data_.get().size(),
                                                cudaMemcpyHostToDevice),
                                     "Failed to copy to tensor data");
    } else {
      std::memcpy(gxf_tensor->pointer(), data_.get().data(), data_.get().size());
    }
  }

  // Create Holoscan tensor
  auto maybe_dl_ctx = (*gxf_tensor).toDLManagedTensorContext();
  if (!maybe_dl_ctx) {
    HOLOSCAN_LOG_ERROR(
        "failed to get std::shared_ptr<DLManagedTensorContext> from nvidia::gxf::Tensor");
  }
  std::shared_ptr<Tensor> holoscan_tensor = std::make_shared<Tensor>(maybe_dl_ctx.value());

  // insert tensor into the TensorMap
  out_message.insert({tensor_name_.get().c_str(), holoscan_tensor});

  op_output.emit(out_message, "out");

  HOLOSCAN_LOG_INFO("Sent message {}", count_++);
}

}  // namespace holoscan::ops
