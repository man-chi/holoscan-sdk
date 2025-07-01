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
#ifndef MODULES_HOLOINFER_SRC_INCLUDE_HOLOINFER_HPP
#define MODULES_HOLOINFER_SRC_INCLUDE_HOLOINFER_HPP

#include <cuda.h>
#include <cuda_runtime.h>
#include <nvrtc.h>

#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "holoinfer_buffer.hpp"

namespace holoscan {
namespace inference {

class ManagerProcessor;

/**
 * Inference Context class
 */
class _HOLOSCAN_EXTERNAL_API_ InferContext {
 public:
  InferContext();
  ~InferContext();
  /**
   * Set Inference parameters
   *
   * @param inference_specs   Pointer to inference specifications
   *
   * @return InferStatus with appropriate holoinfer_code and message.
   */
  InferStatus set_inference_params(std::shared_ptr<InferenceSpecs>& inference_specs);

  /**
   * Executes the inference
   * Toolkit supports one input per model, in float32 type.
   * The provided CUDA stream is used to prepare the input data and will be used to operate on the
   * output data, any execution of CUDA work should be in sync with this stream.
   *
   * @param inference_specs   Pointer to inference specifications
   * @param cuda_stream CUDA stream
   *
   * @return InferStatus with appropriate holoinfer_code and message.
   */
  InferStatus execute_inference(std::shared_ptr<InferenceSpecs>& inference_specs,
                                cudaStream_t cuda_stream = 0);

  /**
   * Gets output dimension per model
   *
   * @return Map of model as key mapped to the output dimension (of inferred data)
   */
  DimType get_output_dimensions() const;

  /**
   * Gets input dimension per model
   *
   * @return Map of model as key mapped to the input dimensions
   */
  DimType get_input_dimensions() const;

 private:
  std::string unique_id_;
};

/**
 * Processor Context class
 */
class _HOLOSCAN_EXTERNAL_API_ ProcessorContext {
 public:
  ProcessorContext();
  /**
   * Initialize the preprocessor context
   *
   * @param process_operations   Map of tensor name as key, mapped to list of operations to be
   *                             applied in sequence on the tensor
   * @param custom_kernels       Map of custom kernel identifier, mapped to related value as a
   *                             string
   * @param use_cuda_graphs       Flag to enable CUDA Graphs for processing custom CUDA kernels
   * @param config_path          Configuration path as a string
   *
   * @return InferStatus with appropriate holoinfer_code and message.
   */
  InferStatus initialize(const MultiMappings& process_operations, const Mappings& custom_kernels,
                         bool use_cuda_graphs, const std::string config_path);

  /**
   * Process the tensors with operations as initialized.
   * Toolkit supports one tensor input and output per model
   *
   * @param tensor_oper_map Map of tensor name as key, mapped to list of operations to be applied in
   * sequence on the tensor
   * @param in_out_tensor_map Map of input tensor name mapped to vector of output tensor names
   * after processing
   * @param processed_result_map Map is updated with output tensor name as key mapped to processed
   * output as a vector of float32 type
   * @param dimension_map Map is updated with model name as key mapped to dimension of processed
   * data as a vector
   * @param process_with_cuda Flag defining if processing should be done with CUDA
   * @param cuda_stream CUDA stream to use when procseeing is done with CUDA
   *
   * @return InferStatus with appropriate holoinfer_code and message.
   */
  InferStatus process(const MultiMappings& tensor_oper_map, const MultiMappings& in_out_tensor_map,
                      DataMap& processed_result_map,
                      const std::map<std::string, std::vector<int>>& dimension_map,
                      bool process_with_cuda, cudaStream_t cuda_stream = 0);

  /**
   * Get output data per Tensor
   * Toolkit supports one output per Tensor, in float32 type
   *
   * @return Map of tensor name as key mapped to the output float32 type data as a vector
   */
  DataMap get_processed_data() const;

  /**
   * Get output dimension per model
   * Toolkit supports one output per model
   *
   * @return Map of model as key mapped to the output dimension (of processed data) as a vector
   */
  DimType get_processed_data_dims() const;

 private:
  /// Pointer to manager class for multi data processing
  std::shared_ptr<ManagerProcessor> process_manager_;
};

}  // namespace inference
}  // namespace holoscan

#endif /* MODULES_HOLOINFER_SRC_INCLUDE_HOLOINFER_HPP */
