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
#ifndef _HOLOSCAN_INFER_MANAGER_H
#define _HOLOSCAN_INFER_MANAGER_H

#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <string>

#include <holoinfer.hpp>
#include <holoinfer_buffer.hpp>
#include <holoinfer_constants.hpp>
#include <holoinfer_utils.hpp>
#include <infer/infer.hpp>
#include <utils/work_queue.hpp>

#if defined(HOLOINFER_ORT_ENABLED)
#include <infer/onnx/core.hpp>
#endif

#if defined(HOLOINFER_TORCH_ENABLED)
#include <infer/torch/core.hpp>
#endif

#include <infer/trt/core.hpp>
#include <params/infer_param.hpp>

namespace holoscan {
namespace inference {
/**
 * @brief Manager class for inference
 */
class ManagerInfer {
 public:
  /**
   * @brief Default Constructor
   */
  ManagerInfer();

  /**
   * @brief Destructor
   */
  ~ManagerInfer();

  /**
   * @brief Create inference settings and memory
   *
   * @param inference_specs specifications for inference
   *
   * @return InferStatus with appropriate code and message
   */
  InferStatus set_inference_params(std::shared_ptr<InferenceSpecs>& inference_specs);

  /**
   * @brief Prepares and launches single/multiple inference
   *
   * The provided CUDA stream is used to prepare the input data and will be used to operate on the
   * output data, any execution of CUDA work should be in sync with this stream.
   *
   * @param inference_specs specifications for inference
   * @param cuda_stream CUDA stream
   *
   * @return InferStatus with appropriate code and message
   */
  InferStatus execute_inference(std::shared_ptr<InferenceSpecs>& inference_specs,
                                cudaStream_t cuda_stream);

  /**
   * @brief Executes Core inference for a particular model and generates inferred data
   * The provided CUDA stream is used to prepare the input data and will be used to operate on the
   * output data, any execution of CUDA work should be in sync with this stream.
   *
   * @param model_name Input model to do the inference on
   * @param permodel_preprocess_data Input DataMap with model name as key and DataBuffer as value
   * @param permodel_output_data Output DataMap with tensor name as key and DataBuffer as value
   * @param cuda_stream CUDA stream
   *
   * @return InferStatus with appropriate code and message
   */
  InferStatus run_core_inference(const std::string& model_name,
                                 const DataMap& permodel_preprocess_data,
                                 const DataMap& permodel_output_data, cudaStream_t cuda_stream);
  /**
   * @brief Cleans up internal context per model
   *
   */
  void cleanup();

  /**
   * @brief Get input dimension per model
   *
   * @return Map with model name as key and dimension as value
   */
  DimType get_input_dimensions() const;

  /**
   * @brief Get output dimension per tensor
   *
   * @return Map with tensor name as key and dimension as value
   */
  DimType get_output_dimensions() const;

 private:
  /// Flag to infer models in parallel. Defaults to False
  bool parallel_processing_ = false;

  /// Flag to demonstrate if input data buffer is on cuda
  bool cuda_buffer_in_ = false;

  /// Flag to demonstrate if output data buffer will be on cuda
  bool cuda_buffer_out_ = false;

  /// @brief Flag to demonstrate if multi-GPU feature has Peer to Peer transfer enabled.
  bool mgpu_p2p_transfer_ = true;

  /// @brief Map to store cuda streams associated with each input tensor in each model on GPU-dt.
  /// Will be used with Multi-GPU feature.
  std::map<std::string, std::map<std::string, cudaStream_t>> input_streams_gpudt_;

  /// @brief Map to store cuda streams associated with each output tensor in each model on GPU-dt.
  /// Will be used with Multi-GPU feature.
  std::map<std::string, std::map<std::string, cudaStream_t>> output_streams_gpudt_;

  /// @brief Map to store cuda streams associated with each input tensor in each model on the
  /// inference device.  Will be used with Multi-GPU feature.
  std::map<std::string, std::map<std::string, cudaStream_t>> input_streams_device_;

  /// @brief Map to store cuda streams associated with each output tensor in each model on the
  /// inference device. Will be used with Multi-GPU feature.
  std::map<std::string, std::map<std::string, cudaStream_t>> output_streams_device_;

  /// @brief Map to store a CUDA event for each device for each model. Will be used with Multi-GPU
  /// feature.
  std::map<std::string, std::map<int, cudaEvent_t>> mgpu_cuda_event_;

  /// Map storing parameters per model
  std::map<std::string, std::unique_ptr<Params>> infer_param_;

  /// Map storing Inference context per model
  std::map<std::string, std::unique_ptr<InferBase>> holo_infer_context_;

  /// Map storing input dimension per model
  DimType models_input_dims_;

  /// Output buffer for multi-GPU inference
  std::map<std::string, DataMap> mgpu_output_buffer_;

  /// Input buffer for multi-gpu inference
  std::map<std::string, DataMap> mgpu_input_buffer_;

  /// Frame counter into the inference engine
  unsigned int frame_counter_ = 0;

  /// Data transfer GPU. Default: 0. Not configurable in this release.
  int device_gpu_dt_ = 0;

  /// CUDA event on data transfer GPU, used to synchronize inference execution with data transfer.
  cudaEvent_t cuda_event_ = nullptr;

  /// Map storing inferred output dimension per tensor
  DimType models_output_dims_;

  /// Work queue use for parallel processing
  std::unique_ptr<WorkQueue> work_queue_;

  /// Map storing Backends supported with holoinfer mapping
  inline static std::map<std::string, holoinfer_backend> supported_backend_{
      {"onnxrt", holoinfer_backend::h_onnx},
      {"trt", holoinfer_backend::h_trt},
      {"torch", holoinfer_backend::h_torch}};
};

/// Pointer to manager class for inference
std::shared_ptr<ManagerInfer> g_manager;

/// Map to store multi-instance managers
std::map<std::string, std::shared_ptr<ManagerInfer>> g_managers;

}  // namespace inference
}  // namespace holoscan

#endif
