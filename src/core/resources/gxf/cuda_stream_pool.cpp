/*
 * SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include "holoscan/core/resources/gxf/cuda_stream_pool.hpp"

#include <cstdint>
#include <memory>
#include <string>

#include "gxf/std/resources.hpp"  // for GPUDevice
#include "holoscan/core/component_spec.hpp"
#include "holoscan/core/gxf/gxf_utils.hpp"
#include "holoscan/core/resources/gxf/cuda_green_context.hpp"

namespace holoscan {

namespace {
constexpr uint32_t kDefaultStreamFlags = 0;
constexpr int32_t kDefaultStreamPriority = 0;
constexpr uint32_t kDefaultReservedSize = 1;
constexpr uint32_t kDefaultMaxSize = 0;
constexpr int32_t kDefaultDeviceId = 0;

}  // namespace

CudaStreamPool::CudaStreamPool(const std::string& name, nvidia::gxf::CudaStreamPool* component)
    : Allocator(name, component) {
  if (!component) {
    throw std::invalid_argument("CudaStreamPool component cannot be null");
  }
  auto maybe_stream_flags = component->getParameter<int32_t>("stream_flags");
  if (!maybe_stream_flags) {
    throw std::runtime_error("Failed to get stream_flags parameter from GXF CudaStreamPool");
  }
  stream_flags_ = maybe_stream_flags.value();

  auto maybe_stream_priority = component->getParameter<int32_t>("stream_priority");
  if (!maybe_stream_priority) {
    throw std::runtime_error("Failed to get stream_priority parameter from GXF CudaStreamPool");
  }
  stream_priority_ = maybe_stream_priority.value();

  auto maybe_reserved_size = component->getParameter<uint32_t>("reserved_size");
  if (!maybe_reserved_size) {
    throw std::runtime_error("Failed to get reserved_size parameter from GXF CudaStreamPool");
  }
  reserved_size_ = maybe_reserved_size.value();

  auto maybe_max_size = component->getParameter<uint32_t>("max_size");
  if (!maybe_max_size) {
    throw std::runtime_error("Failed to get max_size parameter from GXF CudaStreamPool");
  }
  max_size_ = maybe_max_size.value();

  auto maybe_cuda_green_context =
      component->getParameter<std::shared_ptr<CudaGreenContext>>("cuda_green_context");
  cuda_green_context_ = maybe_cuda_green_context ? maybe_cuda_green_context.value() : nullptr;

  auto maybe_gpu_device =
      component->getParameter<nvidia::gxf::Handle<nvidia::gxf::GPUDevice>>("dev_id");
  if (!maybe_gpu_device) {
    throw std::runtime_error("Failed to get dev_id parameter from GXF CudaStreamPool");
  }
  auto gpu_device_handle = maybe_gpu_device.value();
  dev_id_ = gpu_device_handle->device_id();
}

nvidia::gxf::CudaStreamPool* CudaStreamPool::get() const {
  return static_cast<nvidia::gxf::CudaStreamPool*>(gxf_cptr_);
}

void CudaStreamPool::setup(ComponentSpec& spec) {
  // TODO(unknown): The dev_id parameter was removed in GXF 3.0 and replaced with a GPUDevice
  // Resource Note: We are currently working around this with special handling of the "dev_id"
  // parameter in GXFResource::initialize().
  spec.param(
      dev_id_, "dev_id", "Device Id", "Create CUDA Stream on which device.", kDefaultDeviceId);
  spec.param(stream_flags_,
             "stream_flags",
             "Stream Flags",
             "Flags for CUDA streams in the pool. The flag value will be passed to CUDA's "
             "cudaStreamCreateWithPriority when creating the streams. A value of 0 corresponds to "
             "`cudaStreamDefault` while a value of 1 corresponds to `cudaStreamNonBlocking`, "
             "indicating that the stream can run concurrently with work in stream 0 (default "
             "stream) and should not perform any implicit synchronization with it. See: "
             "https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__STREAM.html.",
             kDefaultStreamFlags);
  spec.param(stream_priority_,
             "stream_priority",
             "Stream Priority",
             "Priority of the CUDA streams in the pool. This is an integer value passed to "
             "cudaSreamCreateWithPriority . Lower numbers represent higher priorities. See: "
             "https://docs.nvidia.com/cuda/cuda-runtime-api/group__CUDART__STREAM.html.",
             kDefaultStreamPriority);
  spec.param(reserved_size_,
             "reserved_size",
             "Reserved Stream Size",
             "The number of CUDA streams to initially reserve in the pool"
             " (prior to first request).",
             kDefaultReservedSize);
  spec.param(max_size_,
             "max_size",
             "Maximum Pool Size",
             "The maximum number of streams that can be allocated, unlimited by default",
             kDefaultMaxSize);
  spec.param(cuda_green_context_,
             "cuda_green_context",
             "Cuda Green Context",
             "The green context to use for the CUDA streams in the pool.",
             static_cast<std::shared_ptr<CudaGreenContext>>(nullptr));
}

void CudaStreamPool::initialize() {
  HOLOSCAN_LOG_DEBUG("CudaStreamPool '{}': initialize", name());

  std::shared_ptr<CudaGreenContext> green_context_ptr;
  if (cuda_green_context_.has_value()) {
    green_context_ptr = cuda_green_context_.get();
  }

  if (green_context_ptr != nullptr) {
    if (gxf_eid_ != 0 && green_context_ptr->gxf_eid() == 0) {
      green_context_ptr->gxf_eid(gxf_eid_);
    }
    green_context_ptr->initialize();
  }
  GXFResource::initialize();
}

}  // namespace holoscan
