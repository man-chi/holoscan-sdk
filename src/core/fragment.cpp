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
#include "holoscan/core/fragment.hpp"

#include <yaml-cpp/yaml.h>

#include <functional>
#include <iterator>  // for std::back_inserter
#include <memory>
#include <mutex>  // for std::call_once
#include <set>
#include <string>
#include <typeinfo>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#include "holoscan/core/arg.hpp"
#include "holoscan/core/config.hpp"
#include "holoscan/core/dataflow_tracker.hpp"
#include "holoscan/core/executors/gxf/gxf_executor.hpp"
#include "holoscan/core/graphs/flow_graph.hpp"
#include "holoscan/core/gxf/entity_group.hpp"
#include "holoscan/core/gxf/gxf_network_context.hpp"
#include "holoscan/core/gxf/gxf_scheduler.hpp"
#include "holoscan/core/metadata.hpp"
#include "holoscan/core/operator.hpp"
#include "holoscan/core/resources/gxf/system_resources.hpp"
#include "holoscan/core/schedulers/gxf/greedy_scheduler.hpp"

using std::string_literals::operator""s;

namespace holoscan {

Fragment::~Fragment() {
  // Set `is_run_called_` to true in case the fragment is being destroyed before
  // run() or run_async() has completed execution, enabling proper cleanup in reset_state().
  is_run_called_ = true;

  // Explicitly clean up graph entities.
  reset_graph_entities();
}

Fragment& Fragment::name(const std::string& name) & {
  if (name == "all") {
    HOLOSCAN_LOG_ERROR("Fragment name 'all' is reserved. Please use another name.");
  }
  name_ = name;
  return *this;
}

Fragment&& Fragment::name(const std::string& name) && {
  if (name == "all") {
    HOLOSCAN_LOG_ERROR("Fragment name 'all' is reserved. Please use another name.");
  }
  name_ = name;
  return std::move(*this);
}

const std::string& Fragment::name() const {
  return name_;
}

Fragment& Fragment::application(Application* app) {
  app_ = app;
  return *this;
}

Application* Fragment::application() const {
  return app_;
}

bool Fragment::is_metadata_enabled() const {
  if (is_metadata_enabled_.has_value()) {
    return is_metadata_enabled_.value();
  } else if (app_ != nullptr) {
    return app_->is_metadata_enabled();
  } else {
    return kDefaultMetadataEnabled;
  }
}

void Fragment::enable_metadata(bool enabled) {
  is_metadata_enabled_ = enabled;
}

void Fragment::is_metadata_enabled(bool enabled) {
  static std::once_flag warn_flag;
  std::call_once(warn_flag, []() {
    HOLOSCAN_LOG_WARN(
        "The Fragment::is_metadata_enabled(bool) setter is deprecated. Please use "
        "Fragment::enable_metadata(bool) instead.");
  });
  is_metadata_enabled_ = enabled;
}

MetadataPolicy Fragment::metadata_policy() const {
  if (metadata_policy_.has_value()) {
    return metadata_policy_.value();
  } else if (app_ != nullptr) {
    return app_->metadata_policy();
  } else {
    return kDefaultMetadataPolicy;
  }
}

void Fragment::metadata_policy(MetadataPolicy policy) {
  metadata_policy_ = policy;
}

void Fragment::config(const std::string& config_file, const std::string& prefix) {
  if (config_) { HOLOSCAN_LOG_WARN("Config object was already created. Overwriting..."); }
  if (is_composed_) {
    HOLOSCAN_LOG_WARN(
        "Graph has already been composed. Please make sure that graph composition is not dependent "
        "on this config() call.");
  }

  // If the application is executed with `--config` option or HOLOSCAN_CONFIG_PATH environment,
  // we ignore the config_file argument.
  if (app_) {
    auto& config_path = app_->options().config_path;
    if (!config_path.empty() && config_path.size() > 0) {
      HOLOSCAN_LOG_DEBUG("Configuration path would be overridden by --config option to '{}'",
                         config_path);
      return;
    } else {
      const char* env_value = std::getenv("HOLOSCAN_CONFIG_PATH");
      if (env_value != nullptr && env_value[0] != '\0') {
        HOLOSCAN_LOG_DEBUG(
            "Configuration path would be overridden by HOLOSCAN_CONFIG_PATH "
            "environment variable to '{}'",
            env_value);
        return;
      }
    }
  }

  config_ = make_config<Config>(config_file, prefix);
}

void Fragment::config(std::shared_ptr<Config>& config) {
  if (config_) { HOLOSCAN_LOG_WARN("Config object was already created. Overwriting..."); }
  if (is_composed_) {
    HOLOSCAN_LOG_WARN(
        "Graph has already been composed. Please make sure that graph composition is not dependent "
        "on this config() call.");
  }
  config_ = config;
}

Config& Fragment::config() {
  return *config_shared();
}

std::shared_ptr<Config> Fragment::config_shared() {
  if (!config_) {
    // If the application is executed with `--config` option or HOLOSCAN_CONFIG_PATH environment
    // variable, we take the config file from there.
    if (app_) {
      auto& config_path = app_->options().config_path;
      if (!config_path.empty() && config_path.size() > 0) {
        HOLOSCAN_LOG_DEBUG("Loading config from '{}' (through --config option)", config_path);
        config_ = make_config<Config>(config_path.c_str());
        return config_;
      } else {
        const char* env_value = std::getenv("HOLOSCAN_CONFIG_PATH");
        if (env_value != nullptr && env_value[0] != '\0') {
          HOLOSCAN_LOG_DEBUG(
              "Loading config from '{}' (through HOLOSCAN_CONFIG_PATH environment variable)",
              env_value);
          config_ = make_config<Config>(env_value);
          return config_;
        }
      }
    }
    config_ = make_config<Config>();
  }
  return config_;
}

OperatorGraph& Fragment::graph() {
  return *graph_shared();
}

std::shared_ptr<OperatorGraph> Fragment::graph_shared() {
  if (!graph_) { graph_ = make_graph<OperatorFlowGraph>(); }
  return graph_;
}

Executor& Fragment::executor() {
  return *executor_shared();
}

std::shared_ptr<Executor> Fragment::executor_shared() {
  if (!executor_) { executor_ = make_executor<gxf::GXFExecutor>(); }
  return executor_;
}

void Fragment::scheduler(const std::shared_ptr<Scheduler>& scheduler) {
  scheduler_ = scheduler;
}

std::shared_ptr<Scheduler> Fragment::scheduler() {
  if (!scheduler_) { scheduler_ = make_scheduler<GreedyScheduler>(); }
  return scheduler_;
}

void Fragment::network_context(const std::shared_ptr<NetworkContext>& network_context) {
  network_context_ = network_context;
}

std::shared_ptr<NetworkContext> Fragment::network_context() {
  return network_context_;
}

namespace {  // anonymous details to avoid polluting holoscan namespace

std::unordered_set<std::string> nested_yaml_map_keys_(YAML::Node yaml_node) {
  std::unordered_set<std::string> keys;
  for (auto it = yaml_node.begin(); it != yaml_node.end(); ++it) {
    const auto& key = it->first.as<std::string>();
    const auto& value = it->second;
    keys.emplace(key);
    if (value.IsMap()) {
      std::unordered_set<std::string> inner_keys = nested_yaml_map_keys_(it->second);
      for (const auto& inner_key : inner_keys) { keys.emplace(key + "."s + inner_key); }
    }
  }
  return keys;
}

}  // namespace

std::unordered_set<std::string> Fragment::config_keys() {
  auto& yaml_nodes = config().yaml_nodes();

  std::unordered_set<std::string> all_keys;
  for (const auto& yaml_node : yaml_nodes) {
    if (yaml_node.IsMap()) {
      auto node_keys = nested_yaml_map_keys_(yaml_node);
      for (const auto& k : node_keys) { all_keys.insert(k); }
    }
  }
  return all_keys;
}

ArgList Fragment::from_config(const std::string& key) {
  auto& yaml_nodes = config().yaml_nodes();
  ArgList args;

  std::vector<std::string> key_parts;

  size_t pos = 0;
  while (pos != std::string::npos) {
    size_t next_pos = key.find_first_of('.', pos);
    if (next_pos == std::string::npos) { break; }
    key_parts.push_back(key.substr(pos, next_pos - pos));
    pos = next_pos + 1;
  }
  key_parts.push_back(key.substr(pos));

  size_t key_parts_size = key_parts.size();

  for (const auto& yaml_node : yaml_nodes) {
    if (yaml_node.IsMap()) {
      auto yaml_map = yaml_node.as<YAML::Node>();
      size_t key_index = 0;
      for (const auto& key_part : key_parts) {
        (void)key_part;
        if (yaml_map.IsMap()) {
          yaml_map.reset(yaml_map[key_part]);
          ++key_index;
        } else {
          break;
        }
      }
      if (!yaml_map || key_index < key_parts_size) {
        HOLOSCAN_LOG_ERROR("Unable to find the parameter item/map with key '{}'", key);
        continue;
      }

      const auto& parameters = yaml_map;

      if (parameters.IsScalar()) {
        const std::string& param_key = key_parts[key_parts_size - 1];
        auto& value = parameters;
        args.add(Arg(param_key) = value);
        continue;
      }

      for (const auto& p : parameters) {
        const std::string param_key = p.first.as<std::string>();
        auto& value = p.second;
        args.add(Arg(param_key) = value);
      }
    }
  }

  return args;
}

const std::shared_ptr<Operator>& Fragment::start_op() {
  if (!start_op_) {
    // According to `GxfEntityCreateInfo` definition, the entity name should not start with
    // a double underscore. So, we use this unique name to avoid any conflicts.
    start_op_ = make_operator<Operator>(kStartOperatorName, make_condition<CountCondition>(1));
    add_operator(start_op_);
  }
  return start_op_;
}

void Fragment::add_operator(const std::shared_ptr<Operator>& op) {
  op->set_self_shared(op);
  graph().add_node(op);
}

void Fragment::add_flow(const std::shared_ptr<Operator>& upstream_op,
                        const std::shared_ptr<Operator>& downstream_op) {
  add_flow(upstream_op, downstream_op, {});
}

void Fragment::add_flow(const std::shared_ptr<Operator>& upstream_op,
                        const std::shared_ptr<Operator>& downstream_op,
                        std::set<std::pair<std::string, std::string>> port_pairs) {
  auto port_map = std::make_shared<OperatorEdgeDataElementType>();

  auto upstream_op_spec = upstream_op->spec();
  if (upstream_op_spec == nullptr) {
    HOLOSCAN_LOG_ERROR("upstream_op_spec is nullptr");
    return;
  }
  auto downstream_op_spec = downstream_op->spec();
  if (downstream_op_spec == nullptr) {
    HOLOSCAN_LOG_ERROR("downstream_op_spec is nullptr");
    return;
  }

  auto& op_outputs = upstream_op_spec->outputs();
  auto& op_inputs = downstream_op_spec->inputs();
  if (port_pairs.empty()) {
    // Check if this is a control flow addition.
    // We also allow control flow addition if the the upstream operator has output ports but the
    // downstream operator has no input ports.
    if (op_inputs.empty() || downstream_op->input_exec_spec()) {
      // Both upstream_op and downstream_op should be the native operators
      if (upstream_op->operator_type() != Operator::OperatorType::kNative ||
          downstream_op->operator_type() != Operator::OperatorType::kNative) {
        HOLOSCAN_LOG_ERROR(
            "Both upstream ('{}', type: {}) and downstream ('{}', type: {}) operators should be "
            "native operators to connect execution ports. Please check the operator types. "
            "Ignoring the control flow addition.",
            upstream_op->name(),
            upstream_op->operator_type() == Operator::OperatorType::kNative ? "Native" : "GXF",
            downstream_op->name(),
            downstream_op->operator_type() == Operator::OperatorType::kNative ? "Native" : "GXF");
        return;
      }

      // Add the control flow between the operators
      (*port_map)[Operator::kOutputExecPortName] = {Operator::kInputExecPortName};

      upstream_op->set_self_shared(upstream_op);
      downstream_op->set_self_shared(downstream_op);
      graph().add_flow(upstream_op, downstream_op, port_map);
      // Handle the control flow addition in the executor
      executor().add_control_flow(upstream_op, downstream_op);
      return;
    }
    if (op_outputs.size() > 1) {
      std::vector<std::string> output_labels;
      for (const auto& [key, _] : op_outputs) { output_labels.push_back(key); }

      HOLOSCAN_LOG_ERROR(
          "The upstream operator has more than one output port ({}) so mapping should be "
          "specified explicitly!",
          fmt::join(output_labels, ", "));
      return;
    }
    if (op_inputs.size() > 1) {
      std::vector<std::string> input_labels;
      for (const auto& [key, _] : op_inputs) { input_labels.push_back(key); }
      HOLOSCAN_LOG_ERROR(
          "The downstream operator has more than one input port ({}) so mapping should be "
          "specified "
          "explicitly!",
          fmt::join(input_labels, ", "));
      return;
    }
    port_pairs.emplace("", "");
  }

  std::vector<std::string> output_labels;
  output_labels.reserve(port_pairs.size());

  // Convert port pairs to port map (set<pair<string, string>> -> map<string, set<string>>)
  for (const auto& [key, value] : port_pairs) {
    if (port_map->find(key) == port_map->end()) {
      (*port_map)[key] = std::set<std::string, std::less<>>();
      output_labels.push_back(key);  // maintain the order of output labels
    }
    (*port_map)[key].insert(value);
  }

  // Verify that the upstream & downstream operators have the input and output ports specified by
  // the port_map
  if (op_outputs.size() == 1 && output_labels.size() != 1) {
    HOLOSCAN_LOG_ERROR(
        "The upstream operator({}) has only one port with label '{}' but port_map "
        "specifies {} labels({}) to the upstream operator's output port",
        upstream_op->name(),
        (*op_outputs.begin()).first,
        output_labels.size(),
        fmt::join(output_labels, ", "));
    return;
  }

  for (const auto& output_label : output_labels) {
    if (op_outputs.find(output_label) == op_outputs.end()) {
      if (op_outputs.size() == 1 && output_labels.size() == 1 && output_label == "") {
        // Set the default output port label
        (*port_map)[op_outputs.begin()->first] = std::move((*port_map)[output_label]);
        port_map->erase(output_label);
        // Update the output label
        output_labels[0] = op_outputs.begin()->first;
        break;
      }
      if (op_outputs.empty()) {
        HOLOSCAN_LOG_ERROR(
            "The upstream operator({}) does not have any output port but '{}' was specified in "
            "port_map",
            upstream_op->name(),
            output_label);
        return;
      }

      auto msg_buf = fmt::memory_buffer();
      for (const auto& [label, _] : op_outputs) {
        if (&label == &(op_outputs.begin()->first)) {
          fmt::format_to(std::back_inserter(msg_buf), "{}", label);
        } else {
          fmt::format_to(std::back_inserter(msg_buf), ", {}", label);
        }
      }
      HOLOSCAN_LOG_ERROR(
          "The upstream operator({}) does not have an output port with label '{}'. It should be "
          "one of ({:.{}})",
          upstream_op->name(),
          output_label,
          msg_buf.data(),
          msg_buf.size());
      return;
    }
  }

  for (const auto& output_label : output_labels) {
    auto& input_labels = (*port_map)[output_label];
    if (op_inputs.size() == 1 && input_labels.size() != 1) {
      HOLOSCAN_LOG_ERROR(
          "The downstream operator({}) has only one port with label '{}' but port_map "
          "specifies {} labels({}) to the downstream operator's input port",
          downstream_op->name(),
          (*op_inputs.begin()).first,
          input_labels.size(),
          fmt::join(input_labels, ", "));
      return;
    }

    // Create a vector to maintain the final input labels
    std::vector<std::string> new_input_labels;
    new_input_labels.reserve(input_labels.size());

    for (auto& input_label : input_labels) {
      auto op_input_iter = op_inputs.find(input_label);
      bool is_receivers = false;
      if (op_input_iter != op_inputs.end()) {
        auto& op_input_iospec = op_input_iter->second;
        if (op_input_iospec->queue_size() == static_cast<int64_t>(IOSpec::kAnySize)) {
          is_receivers = true;
        }
      }

      if (is_receivers || op_input_iter == op_inputs.end()) {
        auto input_receivers_label = input_label;
        if (!is_receivers && op_inputs.size() == 1 && input_labels.size() == 1 &&
            input_label == "") {
          // Set the default input port label if the downstream operator has only one input port,
          // the input label is empty, and the queue size is not 'kAnySize'.
          if (op_inputs.begin()->second->queue_size() != static_cast<int64_t>(IOSpec::kAnySize)) {
            new_input_labels.push_back(op_inputs.begin()->first);
            break;
          } else {
            // Set the input_receivers_label to the default input port label
            input_receivers_label = op_inputs.begin()->first;
          }
        }

        // Support for the case where the destination input port label points to the
        // parameter name of the downstream operator, and the parameter type is
        // 'std::vector<holoscan::IOSpec*>'.
        // If we cannot find the input port with the specified label (e.g., 'receivers'),
        // then we need to find a downstream operator's parameter
        // (with 'std::vector<holoscan::IOSpec*>' type) and create a new input port
        // with a specific label ('<parameter name>:<index>'. e.g, 'receivers:0').
        auto& downstream_op_params = downstream_op_spec->params();
        if (downstream_op_params.find(input_receivers_label) != downstream_op_params.end()) {
          auto& downstream_op_param = downstream_op_params.at(input_receivers_label);
          if (std::type_index(downstream_op_param.type()) ==
              std::type_index(typeid(std::vector<holoscan::IOSpec*>))) {
            const std::any& any_value = downstream_op_param.value();
            auto& param = *std::any_cast<Parameter<std::vector<holoscan::IOSpec*>>*>(any_value);
            param.set_default_value();

            std::vector<holoscan::IOSpec*>& iospec_vector = param.get();

            // Create a new input port for this receivers parameter
            bool succeed = executor().add_receivers(
                downstream_op, input_receivers_label, new_input_labels, iospec_vector);
            if (!succeed) {
              HOLOSCAN_LOG_ERROR(
                  "Failed to add receivers to the downstream operator({}) with label '{}'",
                  downstream_op->name(),
                  input_receivers_label);
              return;
            }
            continue;
          }
        }
        if (op_inputs.empty()) {
          HOLOSCAN_LOG_ERROR(
              "The downstream operator({}) does not have any input port but '{}' was "
              "specified in the port_map",
              downstream_op->name(),
              input_receivers_label);
          return;
        }

        auto msg_buf = fmt::memory_buffer();
        for (const auto& [label, _] : op_inputs) {
          if (&label == &(op_inputs.begin()->first)) {
            fmt::format_to(std::back_inserter(msg_buf), "{}", label);
          } else {
            fmt::format_to(std::back_inserter(msg_buf), ", {}", label);
          }
        }
        HOLOSCAN_LOG_ERROR(
            "The downstream operator({}) does not have an input port with label '{}'. It should "
            "be one of ({:.{}})",
            downstream_op->name(),
            input_receivers_label,
            msg_buf.data(),
            msg_buf.size());
        return;
      }

      // Insert the input label as it is to the new_input_labels
      new_input_labels.push_back(input_label);
    }

    // Update the input labels with new_input_labels
    input_labels.clear();
    // Move the new_input_labels to input_labels
    using str_iter = std::vector<std::string>::iterator;
    input_labels.insert(std::move_iterator<str_iter>(new_input_labels.begin()),
                        std::move_iterator<str_iter>(new_input_labels.end()));
    new_input_labels.clear();
    // Do not use 'new_input_labels' after this point
  }

  upstream_op->set_self_shared(upstream_op);
  downstream_op->set_self_shared(downstream_op);
  graph().add_flow(upstream_op, downstream_op, port_map);
}

void Fragment::set_dynamic_flows(
    const std::shared_ptr<Operator>& op,
    const std::function<void(const std::shared_ptr<Operator>&)>& dynamic_flow_func) {
  if (op) { op->set_dynamic_flows(dynamic_flow_func); }
}

void Fragment::compose() {}

void Fragment::run() {
  // Initialize clean state to ensure proper execution and support multiple consecutive runs
  reset_state();

  executor().run(graph());
  is_run_called_ = true;
}

std::future<void> Fragment::run_async() {
  // Initialize clean state to ensure proper execution and support multiple consecutive runs
  reset_state();

  auto future = executor().run_async(graph());
  is_run_called_ = true;
  return future;
}

holoscan::DataFlowTracker& Fragment::track(uint64_t num_start_messages_to_skip,
                                           uint64_t num_last_messages_to_discard,
                                           int latency_threshold, bool is_limited_tracking) {
  if (!data_flow_tracker_) {
    data_flow_tracker_ = std::make_shared<holoscan::DataFlowTracker>();
    data_flow_tracker_->set_skip_starting_messages(num_start_messages_to_skip);
    data_flow_tracker_->set_discard_last_messages(num_last_messages_to_discard);
    data_flow_tracker_->set_skip_latencies(latency_threshold);
    data_flow_tracker_->set_limited_tracking(is_limited_tracking);
  }
  return *data_flow_tracker_;
}

void Fragment::compose_graph() {
  if (is_composed_) {
    HOLOSCAN_LOG_DEBUG("The fragment({}) has already been composed. Skipping...", name());
    return;
  }

  // Load extensions from the config file before composing the graph.
  // (The GXFCodeletOp and GXFComponentResource classes are required to access the underlying GXF
  //  types in the setup() method when composing a graph.)
  load_extensions_from_config();
  compose();
  is_composed_ = true;

  // Protect against the case where no add_operator or add_flow calls were made
  if (!graph_) {
    HOLOSCAN_LOG_ERROR(fmt::format(
        "Fragment '{}' does not have any operators. Please check that there is at least one call to"
        "`add_operator` or `add_flow` during `Fragment::compose`.",
        name()));
    graph();
  }
}

FragmentPortMap Fragment::port_info() const {
  HOLOSCAN_LOG_TRACE("getting port info for fragment: {}", name_);
  FragmentPortMap fragment_port_info;
  if (!is_composed_ || !graph_) {
    HOLOSCAN_LOG_ERROR("The fragment and its graph must be composed before calling port_info");
    return fragment_port_info;
  }
  std::vector<OperatorNodeType> operators = graph_->get_nodes();
  for (auto& op : operators) {
    HOLOSCAN_LOG_TRACE("\toperator: {}", name_, op->name());
    OperatorSpec* op_spec = op->spec();

    // set of input port names
    std::unordered_set<std::string> input_names;
    input_names.reserve(op_spec->inputs().size());
    for (auto& in : op_spec->inputs()) { input_names.insert(in.first); }
    HOLOSCAN_LOG_TRACE("\t\tadded {} inputs", input_names.size());

    // set of output port names
    std::unordered_set<std::string> output_names;
    output_names.reserve(op_spec->outputs().size());
    for (auto& out : op_spec->outputs()) { output_names.insert(out.first); }
    HOLOSCAN_LOG_TRACE("\t\tadded {} outputs", output_names.size());

    // set of multi-receiver parameter names
    std::unordered_set<std::string> receiver_names;
    for (auto& [param_name, param] : op_spec->params()) {
      // receivers parameter type is 'std::vector<holoscan::IOSpec*>'.
      if (std::type_index(param.type()) ==
          std::type_index(typeid(std::vector<holoscan::IOSpec*>))) {
        receiver_names.insert(param_name);
      }
    }
    HOLOSCAN_LOG_TRACE("\t\tadded {} receivers", receiver_names.size());

    fragment_port_info.try_emplace(
        op->name(), std::move(input_names), std::move(output_names), std::move(receiver_names));
  }
  return fragment_port_info;
}

void Fragment::stop_execution(const std::string& op_name) {
  if (!op_name.empty()) {
    // Stop the execution of the operator with the given name
    auto op = graph().find_node(op_name);
    if (op) {
      op->stop_execution();
    } else {
      HOLOSCAN_LOG_WARN("Operator with name '{}' not found, no operator was stopped", op_name);
    }
  } else {
    // Stop the execution of all operators in the fragment in the order they were added to the
    // graph.
    // (`get_nodes()` returns the nodes in the order they were added to the graph.)
    // If needed, we can use more sophisticated termination logic to stop the operators
    // (e.g., monitoring the operator statuses and stopping the operators when they are finished).
    for (auto& op : graph().get_nodes()) { op->stop_execution(); }
  }
}

void Fragment::reset_graph_entities() {
  // Explicitly clean up graph entities. This is necessary for Python apps, because the Python
  // object lifetime may outlive the Application runtime and these must be released prior to the
  // call to `GxfContextDestroy` to avoid a segfault in the `nvidia::gxf::GraphEntity` destructor.
  // This method is invoked by the GXFExecutor::run_gxf_graph() function and during the Fragment
  // destructor to ensure proper cleanup of graph entity resources.
  for (auto& op : graph().get_nodes()) { op->reset_graph_entities(); }
  auto gxf_sch = std::dynamic_pointer_cast<gxf::GXFScheduler>(scheduler());
  if (gxf_sch) { gxf_sch->reset_graph_entities(); }
  auto gxf_network_context = std::dynamic_pointer_cast<gxf::GXFNetworkContext>(network_context());
  if (gxf_network_context) { gxf_network_context->reset_graph_entities(); }
}

void Fragment::reset_state() {
  if (!is_run_called_) {
    HOLOSCAN_LOG_DEBUG(
        "skipping fragment state reset since run() or run_async() was not called yet");
    return;
  }

  // First clean up any graph entities
  reset_graph_entities();

  // If this has a GXFExecutor, we need to reset its flags
  auto gxf_executor = std::dynamic_pointer_cast<gxf::GXFExecutor>(executor_);
  if (gxf_executor) {
    // Reset the execution state (flags for graph initialization and activation)
    gxf_executor->reset_execution_state();
  }

  // Skip resetting the executor here as it needs to remain accessible before
  // run()/run_async() calls. The executor object will be shared between run() method calls.
  // executor_.reset();  // DO NOT RESET THIS.

  // Reset the graph to recreate it on the next run
  graph_.reset();

  // Skip resetting the scheduler since it is shared between run() method calls.
  // scheduler_.reset();  // DO NOT RESET THIS.

  // Skip resetting the network context since it is shared between run() method calls.
  // network_context_.reset();  // DO NOT RESET THIS.

  // Skip resetting data_flow_tracker as its lifecycle is managed outside the run() method.
  // data_flow_tracker_.reset();  // DO NOT RESET THIS.

  // Clear thread pools to prevent memory leaks
  thread_pools_.clear();

  // Reset the is_composed_ flag to ensure graphs are recomposed
  is_composed_ = false;
}

void Fragment::load_extensions_from_config() {
  HOLOSCAN_LOG_INFO("Loading extensions from configs...");
  // Load any extensions that may be present in the config file
  for (const auto& yaml_node : config().yaml_nodes()) {
    executor().extension_manager()->load_extensions_from_yaml(yaml_node);
  }
}

/**
 * @brief Create a new thread pool resource.
 *
 * @param name The name of the resource.
 * @param args The arguments for the resource.
 * @return The shared pointer to the resource.
 */
std::shared_ptr<ThreadPool> Fragment::make_thread_pool(const std::string& name,
                                                       int64_t initial_size) {
  // Create a dedicated GXF Entity for the ThreadPool
  // (unlike a typical Condition/Resource, it does not belong to the same entity as an operator)
  auto pool_entity = std::make_shared<nvidia::gxf::GraphEntity>();
  auto pool_entity_name = fmt::format("{}_{}_entity", this->name(), name);
  auto maybe_pool = pool_entity->setup(executor().context(), pool_entity_name.c_str());
  if (!maybe_pool) {
    throw std::runtime_error(
        fmt::format("Failed to create thread pool entity: '{}'", pool_entity_name));
  }

  // Create the ThreadPool resource
  auto pool_resource = make_resource<ThreadPool>(name, holoscan::Arg("initial_size", initial_size));

  // Assign the pool to the entity that was created above and initialize it via add_to_graph_entity
  pool_resource->gxf_eid(pool_entity->eid());
  pool_resource->add_to_graph_entity(this, pool_entity);

  auto pool_group = std::make_shared<gxf::EntityGroup>(executor().context(),
                                                       fmt::format("{}_group", pool_entity_name));
  pool_resource->entity_group(std::move(pool_group));

  // Add this ThreadPool into the entity group
  pool_resource->entity_group()->add(*pool_resource);

  // Store pointers to all thread pools so initialization of entity groups can be
  // performed later by GXFExecutor. We can only add operators to the entity group AFTER they have
  // been initialized in GXFExecutor.
  thread_pools_.push_back(pool_resource);

  return pool_resource;
}

}  // namespace holoscan
