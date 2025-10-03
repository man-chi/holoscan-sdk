/*
 * SPDX-FileCopyrightText: Copyright (c) 2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#include <gtest/gtest.h>
#include <cuda.h>

#include <memory>
#include <string>
#include <vector>

#include <holoscan/holoscan.hpp>
#include <holoscan/core/execution_context.hpp>
#include <holoscan/operators/inference/inference.hpp>
#include <holoscan/utils/cuda_macros.hpp>

// Test tensor dimensions BATCH_SIZE x TENSOR_SIZE x TENSOR_SIZE
constexpr int TENSOR_SIZE = 256;
constexpr int BATCH_SIZE = 1;

namespace holoscan::ops {

class TensorGeneratorOp : public Operator {
 public:
  HOLOSCAN_OPERATOR_FORWARD_ARGS(TensorGeneratorOp)

  TensorGeneratorOp() = default;

  void setup(OperatorSpec& spec) override {
    spec.output<holoscan::gxf::Entity>("output");
    spec.param(allocator_, "allocator", "Allocator", "Allocator");
    spec.param(fill_value_, "fill_value", "Fill value", "Fill value", 0.f);
  }

  void compute([[maybe_unused]] InputContext& op_input, OutputContext& op_output,
               [[maybe_unused]] ExecutionContext& context) override {
    auto entity = holoscan::gxf::Entity::New(&context);

    auto allocator = nvidia::gxf::Handle<nvidia::gxf::Allocator>::Create(context.context(),
                                                                         allocator_->gxf_cid());
    auto tensor =
        static_cast<nvidia::gxf::Entity&>(entity).add<nvidia::gxf::Tensor>("tensor").value();
    nvidia::gxf::Shape shape({BATCH_SIZE, TENSOR_SIZE, TENSOR_SIZE});
    tensor->reshape<float>(shape, nvidia::gxf::MemoryStorageType::kDevice, allocator.value());
    std::vector<float> tensor_data;
    size_t tensor_size = BATCH_SIZE * TENSOR_SIZE * TENSOR_SIZE;
    tensor_data.resize(tensor_size);
    for (int i = 0; i < tensor_size; i++) {
      tensor_data[i] = fill_value_ + static_cast<float>(i);
    }

    HOLOSCAN_CUDA_CALL(cudaMemcpy(tensor->pointer(),
                                  tensor_data.data(),
                                  tensor_size * sizeof(float),
                                  cudaMemcpyHostToDevice));

    op_output.emit(entity, "output");
  }

 private:
  Parameter<std::shared_ptr<Allocator>> allocator_;
  Parameter<float> fill_value_;
};

class ResultCheckerOp : public holoscan::Operator {
 public:
  explicit ResultCheckerOp(bool enable_green_context)
      : enable_green_context_(enable_green_context) {}

  void setup(OperatorSpec& spec) override {
    spec.input<holoscan::gxf::Entity>("input");

    if (enable_green_context_) {
      int version = 0;
      CUresult result = cuDriverGetVersion(&version);
      if (result == CUDA_SUCCESS) {
        result = cuGetProcAddress("cuStreamGetGreenCtx",
                                  reinterpret_cast<void**>(&fnCuStreamGetGreenCtx),
                                  version,
                                  0,
                                  nullptr);
        if (result != CUDA_SUCCESS) {
          const char* error_string;
          cuGetErrorString(result, &error_string);
          HOLOSCAN_LOG_WARN(
              "Failed (error code: {}) getting cuStreamGetGreenCtx() from CUDA driver {}",
              error_string,
              version);
        }
      }
    }
  }

  static void read_data(holoscan::gxf::Entity& entity, const std::string& name,
                        std::vector<float>& out) {
    size_t tensor_size = BATCH_SIZE * TENSOR_SIZE * TENSOR_SIZE;
    out.resize(tensor_size, 0.F);
    auto tensor = entity.get<holoscan::Tensor>(name.c_str());
    if (!tensor) {
      throw std::runtime_error(fmt::format("Tensor '{}' not found in message", name));
    }
    HOLOSCAN_CUDA_CALL(cudaMemcpy(
        out.data(), tensor->data(), tensor_size * sizeof(float), cudaMemcpyDeviceToHost));
  }

  void compute([[maybe_unused]] InputContext& op_input, OutputContext& op_output,
               [[maybe_unused]] ExecutionContext& context) override {
    auto maybe_message = op_input.receive<holoscan::gxf::Entity>("input");
    if (!maybe_message) {
      std::string err_msg =
          fmt::format("No input message received by inference operator on port 'receivers': {}",
                      maybe_message.error().what());
      HOLOSCAN_LOG_ERROR(err_msg);
      throw std::runtime_error(err_msg);
    }

    auto streams = op_input.receive_cuda_streams("input");
    if (streams.empty()) {
      throw std::runtime_error("No CUDA stream found in message");
    }
    cudaStream_t stream = nullptr;
    if (streams.size() > 0 && streams[0].has_value()) {
      stream = streams[0].value();
      cudaStreamSynchronize(stream);
      // Check if the cuda stream is associated with a green context
      if (fnCuStreamGetGreenCtx != nullptr) {
        CUgreenCtx green_context;
        fnCuStreamGetGreenCtx(stream, &green_context);
        if (green_context == nullptr) {
          throw std::runtime_error("Stream is expected to be associated with a green context");
        }
      }
    }

    // Data validation
    std::vector<float> data;
    size_t tensor_size = BATCH_SIZE * TENSOR_SIZE * TENSOR_SIZE;
    read_data(maybe_message.value(), "tensor", data);
    for (int i = 0; i < tensor_size; i++) {
      ASSERT_EQ(data[i], static_cast<float>(i));
    }

    HOLOSCAN_LOG_INFO("Inference result verified");
  }

 private:
  bool enable_green_context_;
  CUresult (*fnCuStreamGetGreenCtx)(CUstream, CUgreenCtx*) = nullptr;
};

}  // namespace holoscan::ops

// Test app for inference operator using flow from tensor generator to inference to result checker
// to verify the inference(identity model) result.
// The identity model is a simple model that takes a tensor as input and returns the same tensor.
// The test tensor has a dimension of BATCH_SIZE x TENSOR_SIZE x TENSOR_SIZE.
class InferenceOpTestApp : public holoscan::Application {
 public:
  explicit InferenceOpTestApp(std::string backend, std::string model_path,
                              bool enable_green_context)
      : backend_(backend), model_path_(model_path), enable_green_context_(enable_green_context) {}

  void compose() override {
    using namespace holoscan;
    auto allocator = make_resource<UnboundedAllocator>("pool");

    // Create a CUDA stream pool
    std::shared_ptr<CudaGreenContextPool> cuda_green_context_pool = nullptr;
    std::shared_ptr<CudaGreenContext> cuda_green_context = nullptr;

    if (enable_green_context_) {
      std::vector<uint32_t> partitions = {4, 4};
      cuda_green_context_pool = make_resource<CudaGreenContextPool>(
          "green_context_pool", 0, 0, partitions.size(), partitions);
      cuda_green_context =
          make_resource<CudaGreenContext>("green_context", cuda_green_context_pool);
    }

    auto cuda_stream_pool =
        make_resource<CudaStreamPool>("stream_pool", 0, 0, 0, 1, 5, cuda_green_context);

    auto tensor_generator_op = make_operator<ops::TensorGeneratorOp>(
        "tensor_generator", Arg("allocator") = allocator, make_condition<CountCondition>(10));

    ops::InferenceOp::DataMap model_path_map;
    std::string model_path = "../tests/operators/inference/models/" + model_path_;
    model_path_map.insert("first", model_path);
    HOLOSCAN_LOG_INFO("model_path = {}", model_path);
    std::vector<int> in_tensor_dimensions = {BATCH_SIZE, TENSOR_SIZE, TENSOR_SIZE};

    auto infer_op =
        make_operator<ops::InferenceOp>("infer",
                                        from_config("inference"),
                                        Arg("backend") = backend_,
                                        Arg("model_path_map") = model_path_map,
                                        Arg("allocator") = allocator,
                                        Arg("in_tensor_dimensions") = in_tensor_dimensions,
                                        cuda_stream_pool);
    auto result_checker_op = make_operator<ops::ResultCheckerOp>("checker", enable_green_context_);

    // Add flow
    add_flow(tensor_generator_op, infer_op, {{"output", "receivers"}});
    add_flow(infer_op, result_checker_op, {{"transmitter", "input"}});
  }

 private:
  std::string backend_;
  std::string model_path_;
  bool enable_green_context_;
};

class InferenceOpTestFixture
    : public ::testing::TestWithParam<std::tuple<std::string, std::string, bool>> {};

TEST_P(InferenceOpTestFixture, InferenceOpTestApp) {
  using namespace holoscan;

  auto& [backend, model, enable_green_context] = GetParam();
  HOLOSCAN_LOG_INFO("backend = {}", backend);
  HOLOSCAN_LOG_INFO("model = {}", model);
  HOLOSCAN_LOG_INFO("enable_green_context = {}", enable_green_context);

  std::filesystem::path config_path;
  config_path = std::filesystem::path("../tests/operators/inference/inference.yaml");

  auto app = make_application<InferenceOpTestApp>(backend, model, enable_green_context);
  app->config(config_path);

  // capture output to check that the expected messages were logged
  testing::internal::CaptureStderr();

  app->run();

  std::string log_output = testing::internal::GetCapturedStderr();

  ASSERT_TRUE(log_output.find("Inference result verified") != std::string::npos);
}

INSTANTIATE_TEST_CASE_P(InferenceOpTestApp, InferenceOpTestFixture,
                        ::testing::Values(std::make_tuple("onnxrt", "identity_model.onnx", false),
                                          std::make_tuple("onnxrt", "identity_model.onnx", true),
                                          std::make_tuple("trt", "identity_model.onnx", false),
                                          std::make_tuple("trt", "identity_model.onnx", true),
                                          std::make_tuple("torch", "identity_model.pt", false),
                                          std::make_tuple("torch", "identity_model.pt", true)));
