"""
SPDX-FileCopyrightText: Copyright (c) 2022-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
SPDX-License-Identifier: Apache-2.0

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
"""  # noqa: E501

import pytest

from holoscan import as_tensor
from holoscan.core import (
    Application,
    Arg,
    ArgContainerType,
    ArgElementType,
    ArgList,
    ArgType,
    CLIOptions,
    Component,
    Condition,
    ConditionType,
    Config,
    Executor,
    Fragment,
    FragmentGraph,
    Graph,
    InputContext,
    IOSpec,
    MultiMessageConditionInfo,
    NetworkContext,
    Operator,
    OperatorGraph,
    OperatorSpec,
    OutputContext,
    Resource,
    Scheduler,
    _Fragment,
    arglist_to_kwargs,
    io_type_registry,
    py_object_to_arg,
)
from holoscan.core._core import ComponentSpec as ComponentSpecBase
from holoscan.core._core import OperatorSpec as OperatorSpecBase
from holoscan.core._core import ParameterFlag, PyOperatorSpec
from holoscan.executors import GXFExecutor
from holoscan.graphs import FlowGraph, OperatorFlowGraph
from holoscan.resources import (
    DoubleBufferReceiver,
    DoubleBufferTransmitter,
    UcxReceiver,
    UcxTransmitter,
)


class OpTx(Operator):
    def __init__(self, *args, **kwargs):
        self.index = 0
        # Need to call the base class constructor last
        super().__init__(*args, **kwargs)

    def setup(self, spec: OperatorSpec):
        spec.output("tensor")


class OpRx(Operator):
    def __init__(self, *args, **kwargs):
        self.index = 0
        # Need to call the base class constructor last
        super().__init__(*args, **kwargs)

    def setup(self, spec: OperatorSpec):
        spec.input("in1")
        spec.input("in2")


def get_tx_and_rx_ops(_fragment):
    op_tx = OpTx(_fragment, name="op_tx")
    op_rx = OpRx(_fragment, name="op_rx")

    return op_tx, op_rx


class TestArgType:
    def test_empty_init(self):
        t = ArgType()
        assert t.container_type == ArgContainerType.NATIVE
        assert t.element_type == ArgElementType.CUSTOM

    def test_init(self):
        t = ArgType(ArgElementType.FLOAT64, ArgContainerType.ARRAY)
        assert t.container_type == ArgContainerType.ARRAY
        assert t.element_type == ArgElementType.FLOAT64

    def test_repr(self):
        t = ArgType(ArgElementType.FLOAT64, ArgContainerType.ARRAY)
        assert repr(t) == "std::array<double,N>"

    def test_dynamic_attribute_not_allowed(self):
        obj = ArgType()
        with pytest.raises(AttributeError):
            obj.custom_attribute = 5


class TestMultiMessageConditionInfo:
    def test_default_constructor(self):
        m = MultiMessageConditionInfo()
        assert m.kind == ConditionType.NONE
        assert m.port_names == []
        assert m.args.size == 0


class TestArg:
    def test_name(self):
        a = Arg("arg_1")
        assert a.name == "arg_1"

        with pytest.raises(TypeError):
            Arg()

    def test_arg_type(self):
        a = Arg("arg_1")
        isinstance(a.arg_type, ArgType)
        isinstance(a.arg_type.container_type, ArgContainerType)
        isinstance(a.arg_type.element_type, ArgElementType)

        assert a.arg_type.container_type == ArgContainerType.NATIVE
        assert a.arg_type.element_type == ArgElementType.CUSTOM

        # Note: have not implemented assignment operator of Arg as it is
        #       unlikely users need to create these directly from Python

    def test_has_value(self):
        a = Arg("my_arg")
        assert not a.has_value

    def test_repr(self):
        a = Arg("my_arg")
        s = a.__repr__()
        assert s == "name: my_arg\ntype: CustomType"

    def test_dynamic_attribute_not_allowed(self):
        obj = Arg("a")
        with pytest.raises(AttributeError):
            obj.custom_attribute = 5

    def test_int_cast(self):
        arg = py_object_to_arg(5, name="num_iter")
        assert isinstance(arg, Arg)
        assert int(arg) == 5

    def test_bool_cast(self):
        arg = py_object_to_arg(True, name="verbose")
        assert isinstance(arg, Arg)
        assert bool(arg)

    def test_float_cast(self):
        arg = py_object_to_arg(5.5, name="beta")
        assert isinstance(arg, Arg)
        assert float(arg) == 5.5

    def test_str_cast(self):
        arg = py_object_to_arg("abc", name="name")
        assert isinstance(arg, Arg)
        assert str(arg) == "abc"


class TestArgList:
    def test_init(self):
        args = ArgList()
        assert args.name == "arglist"
        assert args.size == 0

    def test_add_and_clear(self):
        args = ArgList()
        args.add(Arg("alpha"))
        args.add(Arg("beta"))
        assert args.size == 2

        args2 = ArgList()
        args2.add(Arg("theta"))
        args2.add(args)
        assert args2.size == 3

        args.clear()
        assert args.size == 0
        assert args2.size == 3

    def test_dynamic_attribute_not_allowed(self):
        obj = ArgList()
        with pytest.raises(AttributeError):
            obj.custom_attribute = 5


class TestComponentSpecBase:
    def test_init(self, fragment):
        c = ComponentSpecBase(fragment)
        assert c.params == {}
        assert c.fragment is fragment

    def test_dynamic_attribute_not_allowed(self, fragment):
        obj = ComponentSpecBase(fragment)
        with pytest.raises(AttributeError):
            obj.custom_attribute = 5


class TestComponent:
    def test_init(self):
        c = Component()
        assert c.name == ""
        assert c.fragment is None
        assert c.id == -1

    def test_add_arg(self):
        c = Component()
        c.add_arg(Arg("a1"))

    def test_initialize(self):
        c = Component()
        c.initialize()

    def test_dynamic_attribute_allowed(self):
        obj = Component()
        with pytest.raises(AttributeError):
            obj.custom_attribute = 5


class TestCondition:
    def test_init(self, fragment):
        c = Condition(fragment)
        assert c.name == ""
        assert c.fragment is not None

    def test_init_with_kwargs(self, fragment):
        c = Condition(fragment, a=5, b=(13.7, 15.2), c="abcd")
        assert c.name == ""
        assert c.fragment is not None
        assert len(c.args) == 3

    def test_init_with_name_and_kwargs(self, fragment):
        # name provided by kwarg
        c = Condition(fragment, name="c2", a=5, b=(13.7, 15.2), c="abcd")
        assert c.name == "c2"
        assert c.fragment is not None
        assert len(c.args) == 3

    def test_name(self, fragment):
        c = Condition(fragment)
        c.name = "cond1"
        assert c.name == "cond1"

        c = Condition(fragment, name="cond3")
        assert c.name == "cond3"

    def test_fragment(self, fragment):
        c = Condition(fragment)
        assert c.fragment is not None
        # not allowed to assign fragment
        with pytest.raises(AttributeError):
            c.fragment = fragment

    def test_add_arg(self, fragment):
        c = Condition(fragment)
        c.add_arg(Arg("a1"))

    def test_initialize(self, fragment):
        c = Condition(fragment)
        c.initialize()

    def test_setup(self, fragment):
        spec = ComponentSpecBase(fragment=fragment)
        c = Condition(fragment)
        c.setup(spec)

    def test_dynamic_attribute_allowed(self, fragment):
        obj = Condition(fragment)
        obj.custom_attribute = 5


class TestResource:
    def test_init(self, fragment):
        r = Resource(fragment)
        assert r.name == ""
        assert r.fragment is fragment
        assert r.resource_type == Resource.ResourceType.NATIVE

    def test_init_with_kwargs(self, fragment):
        r = Resource(fragment, a=5, b=(13.7, 15.2), c="abcd")
        assert r.name == ""
        assert r.fragment is fragment
        assert len(r.args) == 3

    def test_init_with_name_and_kwargs(self, fragment):
        # name provided by kwarg
        r = Resource(fragment, name="r2", a=5, b=(13.7, 15.2), c="abcd")
        assert r.name == "r2"
        assert r.fragment is fragment
        assert len(r.args) == 3

    def test_name(self, fragment):
        r = Resource(fragment)
        r.name = "res1"
        assert r.name == "res1"

        r = Resource(fragment, name="res3")
        assert r.name == "res3"

    def test_fragment(self, fragment):
        r = Resource(fragment)
        assert r.fragment is fragment
        # not allowed to assign fragment
        with pytest.raises(AttributeError):
            r.fragment = fragment

    def test_add_arg(self, fragment):
        r = Resource(fragment)
        r.add_arg(Arg("a1"))

    def test_initialize(self, fragment):
        r = Resource(fragment)
        r.initialize()

    def test_setup(self, fragment):
        spec = ComponentSpecBase(fragment=fragment)
        r = Resource(fragment)
        r.setup(spec)

    def test_dynamic_attribute_allowed(self, fragment):
        obj = Resource(fragment)
        obj.custom_attribute = 5


class TestOperatorSpecBase:
    def test_init(self, fragment):
        spec = OperatorSpecBase(fragment)
        assert spec.params == {}
        assert spec.fragment is fragment

    def test_input(self, fragment, capfd):
        spec = OperatorSpecBase(fragment)
        iospec = spec.input()
        assert isinstance(iospec, IOSpec)
        assert iospec.name == "__iospec_input"
        assert iospec.io_type == IOSpec.IOType.INPUT
        assert iospec.queue_size == int(IOSpec.IOSize(1))

        iospec2 = spec.input("input2")
        assert iospec2.name == "input2"
        assert iospec.io_type == IOSpec.IOType.INPUT

        # Calling a second time with the same name will log an error to the
        # console.
        iospec2 = spec.input("input2")
        captured = capfd.readouterr()
        assert "error" in captured.err
        assert "already exists" in captured.err

    def test_input_condition_none(self, fragment):
        spec = OperatorSpecBase(fragment)
        iospec = spec.input("input_no_condition").condition(ConditionType.NONE)
        assert isinstance(iospec, IOSpec)
        assert iospec.name == "input_no_condition"
        assert iospec.io_type == IOSpec.IOType.INPUT
        assert iospec.conditions == [(ConditionType.NONE, None)]

    def test_input_condition_message_available(self, fragment):
        spec = OperatorSpecBase(fragment)
        iospec = spec.input("input_message_available_condition").condition(
            ConditionType.MESSAGE_AVAILABLE, min_size=1
        )
        assert isinstance(iospec, IOSpec)
        assert iospec.name == "input_message_available_condition"
        assert iospec.io_type == IOSpec.IOType.INPUT
        assert len(iospec.conditions) == 1
        assert iospec.conditions[0][0] == ConditionType.MESSAGE_AVAILABLE
        assert iospec.conditions[0][1] is not None

    def test_input_connector_default(self, fragment):
        spec = OperatorSpecBase(fragment)
        iospec = spec.input("input_no_condition").connector(IOSpec.ConnectorType.DEFAULT)
        assert isinstance(iospec, IOSpec)
        assert iospec.name == "input_no_condition"
        assert iospec.io_type == IOSpec.IOType.INPUT
        assert iospec.connector() is None

    @pytest.mark.parametrize("kwargs", [{}, dict(capacity=4), dict(capacity=1, policy=1)])
    def test_input_connector_double_buffer(self, fragment, kwargs):
        spec = OperatorSpecBase(fragment)
        iospec = spec.input("input_no_condition").connector(
            IOSpec.ConnectorType.DOUBLE_BUFFER, **kwargs
        )
        assert isinstance(iospec, IOSpec)
        assert iospec.name == "input_no_condition"
        assert iospec.io_type == IOSpec.IOType.INPUT
        assert isinstance(iospec.connector(), DoubleBufferReceiver)

    @pytest.mark.parametrize(
        "kwargs", [{}, dict(capacity=4), dict(capacity=1, policy=1, address="0.0.0.0", port=13337)]
    )
    def test_input_connector_ucx(self, fragment, kwargs):
        spec = OperatorSpecBase(fragment)
        iospec = spec.input("input_no_condition").connector(IOSpec.ConnectorType.UCX, **kwargs)
        assert isinstance(iospec, IOSpec)
        assert iospec.name == "input_no_condition"
        assert iospec.io_type == IOSpec.IOType.INPUT
        assert isinstance(iospec.connector(), UcxReceiver)

    def test_input_connector_and_condition(self, fragment):
        spec = OperatorSpecBase(fragment)
        iospec = spec.input("in").connector(
            IOSpec.ConnectorType.DOUBLE_BUFFER,
            capacity=5,
            policy=1,
        )
        b = iospec.condition(
            ConditionType.EXPIRING_MESSAGE_AVAILABLE,
            max_batch_size=5,
            max_delay_n=1_000_000_000,
        )

        assert iospec == b

        assert isinstance(iospec, IOSpec)
        assert iospec.name == "in"
        assert iospec.io_type == IOSpec.IOType.INPUT
        assert isinstance(iospec.connector(), DoubleBufferReceiver)

        assert len(iospec.conditions) == 1
        assert iospec.conditions[0][0] == ConditionType.EXPIRING_MESSAGE_AVAILABLE
        assert iospec.conditions[0][1] is not None

        assert spec.inputs["in"] == iospec
        assert len(spec.inputs["in"].conditions) == 1

    def test_input_condition_and_connector(self, fragment):
        spec = OperatorSpecBase(fragment)
        iospec = (
            spec.input("in")
            .condition(
                ConditionType.EXPIRING_MESSAGE_AVAILABLE,
                max_batch_size=5,
                max_delay_n=1_000_000_000,
            )
            .connector(
                IOSpec.ConnectorType.DOUBLE_BUFFER,
                capacity=5,
                policy=1,
            )
        )
        assert isinstance(iospec, IOSpec)
        assert iospec.name == "in"
        assert iospec.io_type == IOSpec.IOType.INPUT
        assert isinstance(iospec.connector(), DoubleBufferReceiver)

        assert len(iospec.conditions) == 1
        assert iospec.conditions[0][0] == ConditionType.EXPIRING_MESSAGE_AVAILABLE
        assert iospec.conditions[0][1] is not None

        assert spec.inputs["in"] == iospec

        assert len(spec.inputs["in"].conditions) == 1

    @pytest.mark.parametrize(
        "kind",
        [ConditionType.MULTI_MESSAGE_AVAILABLE, ConditionType.MULTI_MESSAGE_AVAILABLE_TIMEOUT],
    )
    @pytest.mark.parametrize(
        "sampling_mode",
        ["SumOfAll", "PerReceiver"],
    )
    def test_multi_port_condition(self, fragment, kind, sampling_mode):
        spec = OperatorSpecBase(fragment)
        spec.input("in1")
        spec.input("in2")
        spec.input("in3")

        if sampling_mode == "SumOfAll":
            extra_kwargs = dict(min_sum=4)
        elif sampling_mode == "PerReceiver":
            extra_kwargs = dict(min_sizes=[1, 2, 1])

        spec.multi_port_condition(
            kind,
            port_names=["in1", "in3"],
            sampling_mode=sampling_mode,
            **extra_kwargs,
        )

        # check that the condition was added
        multi_port_conditions = spec.multi_port_conditions()
        assert len(multi_port_conditions) == 1
        for c in multi_port_conditions:
            assert isinstance(c, MultiMessageConditionInfo)

        # check the info on the condition
        multi_port_condition_info = multi_port_conditions[0]
        assert multi_port_condition_info.kind == kind
        assert multi_port_condition_info.port_names == ["in1", "in3"]

        # check that the expected kwargs are present in each case
        kwargs = arglist_to_kwargs(multi_port_condition_info.args)
        if sampling_mode == "SumOfAll":
            assert kwargs["min_sum"] == 4
        elif sampling_mode == "PerReceiver":
            assert kwargs["min_sizes"] == [1, 2, 1]
        assert kwargs["sampling_mode"] == sampling_mode

    @pytest.mark.parametrize(
        "spec_args,spec_kwargs,expected_name,expected_size",
        [
            ([], {}, "__iospec_input", 1),
            (["receivers"], dict(size=IOSpec.SIZE_ONE), "receivers", 1),
            (["receivers"], dict(size=IOSpec.ANY_SIZE), "receivers", int(IOSpec.ANY_SIZE)),
            (
                ["receivers"],
                dict(size=IOSpec.PRECEDING_COUNT),
                "receivers",
                int(IOSpec.PRECEDING_COUNT),
            ),
            (["receivers"], dict(size=5), "receivers", 5),
            (["receivers"], dict(size=IOSpec.IOSize(3)), "receivers", 3),
            (["receivers"], dict(size=IOSpec.IOSize(-3)), "receivers", -3),
        ],
    )
    def test_input_queue_size(
        self, fragment, capfd, spec_args, spec_kwargs, expected_name, expected_size
    ):
        spec = OperatorSpecBase(fragment)
        iospec = spec.input(*spec_args, **spec_kwargs)
        assert isinstance(iospec, IOSpec)
        assert iospec.name == expected_name
        assert iospec.io_type == IOSpec.IOType.INPUT
        assert iospec.queue_size == expected_size

        # Calling a second time with the same name will log an error to the
        # console.
        spec.input(expected_name)
        captured = capfd.readouterr()
        assert "error" in captured.err
        assert "already exists" in captured.err

    def test_output(self, fragment, capfd):
        spec = OperatorSpecBase(fragment)
        iospec = spec.output()
        assert isinstance(iospec, IOSpec)
        assert iospec.name == "__iospec_output"
        assert iospec.io_type == IOSpec.IOType.OUTPUT

        iospec2 = spec.input("output2")
        assert iospec2.name == "output2"
        assert iospec.io_type == IOSpec.IOType.OUTPUT

        # Calling a second time with the same name will log an error
        iospec2 = spec.input("output2")
        captured = capfd.readouterr()
        assert "error" in captured.err
        assert "already exists" in captured.err

    def test_output_condition_none(self, fragment):
        spec = OperatorSpecBase(fragment)
        iospec = spec.output("output_no_condition").condition(ConditionType.NONE)
        assert isinstance(iospec, IOSpec)
        assert iospec.name == "output_no_condition"
        assert iospec.io_type == IOSpec.IOType.OUTPUT
        assert iospec.conditions == [(ConditionType.NONE, None)]

    def test_output_condition_downstream_message_affordable(self, fragment):
        spec = OperatorSpecBase(fragment)
        iospec = spec.output("output_downstream_message_affordable_condition").condition(
            ConditionType.DOWNSTREAM_MESSAGE_AFFORDABLE, min_size=1
        )
        assert isinstance(iospec, IOSpec)
        assert iospec.name == "output_downstream_message_affordable_condition"
        assert iospec.io_type == IOSpec.IOType.OUTPUT
        assert len(iospec.conditions) == 1
        assert iospec.conditions[0][0] == ConditionType.DOWNSTREAM_MESSAGE_AFFORDABLE
        assert iospec.conditions[0][1] is not None

    def test_output_connector_default(self, fragment):
        spec = OperatorSpecBase(fragment)
        iospec = spec.output("output_no_condition").connector(IOSpec.ConnectorType.DEFAULT)
        assert isinstance(iospec, IOSpec)
        assert iospec.name == "output_no_condition"
        assert iospec.io_type == IOSpec.IOType.OUTPUT
        assert iospec.connector() is None

    @pytest.mark.parametrize("kwargs", [{}, dict(capacity=4), dict(capacity=1, policy=1)])
    def test_output_connector_double_buffer(self, fragment, kwargs):
        spec = OperatorSpecBase(fragment)
        iospec = spec.output("output_no_condition").connector(
            IOSpec.ConnectorType.DOUBLE_BUFFER, **kwargs
        )
        assert isinstance(iospec, IOSpec)
        assert iospec.name == "output_no_condition"
        assert iospec.io_type == IOSpec.IOType.OUTPUT
        assert isinstance(iospec.connector(), DoubleBufferTransmitter)

    @pytest.mark.parametrize(
        "kwargs", [{}, dict(capacity=4), dict(capacity=1, policy=1, address="0.0.0.0", port=13337)]
    )
    def test_output_connector_ucx(self, fragment, kwargs):
        spec = OperatorSpecBase(fragment)
        iospec = spec.output("output_no_condition").connector(IOSpec.ConnectorType.UCX, **kwargs)
        assert isinstance(iospec, IOSpec)
        assert iospec.name == "output_no_condition"
        assert iospec.io_type == IOSpec.IOType.OUTPUT
        assert isinstance(iospec.connector(), UcxTransmitter)

    def test_dynamic_attribute_not_allowed(self, fragment):
        obj = OperatorSpecBase(fragment)
        with pytest.raises(AttributeError):
            obj.custom_attribute = 5

    def test_optional_parameter(self, fragment):
        op_tx, _ = get_tx_and_rx_ops(fragment)
        spec = PyOperatorSpec(fragment, op_tx)
        spec.param("optional_param", 5, flag=ParameterFlag.OPTIONAL)


class TestInputContext:
    def test_init_not_allowed(self):
        # abstract base class can't be initialized
        with pytest.raises(TypeError):
            InputContext()


class TestOutputContext:
    def test_init_not_allowed(self):
        # abstract base class can't be initialized
        with pytest.raises(TypeError):
            OutputContext()


def test_condition_type():
    # just verifies that the various enums exist
    (  # noqa: B018
        ConditionType.NONE,
        ConditionType.MESSAGE_AVAILABLE,
        ConditionType.EXPIRING_MESSAGE_AVAILABLE,
        ConditionType.MULTI_MESSAGE_AVAILABLE,
        ConditionType.MULTI_MESSAGE_AVAILABLE_TIMEOUT,
        ConditionType.DOWNSTREAM_MESSAGE_AFFORDABLE,
        ConditionType.COUNT,
        ConditionType.BOOLEAN,
        ConditionType.PERIODIC,
        ConditionType.ASYNCHRONOUS,
    )


class TestConfig:
    def test_init_nonexistent(self, capfd):
        # The following will log a warning to the console
        conf = Config(config_file="nonexistent-file", prefix="")
        captured = capfd.readouterr()
        assert isinstance(conf, Config)
        assert "warning" in captured.err
        assert "Config file 'nonexistent-file' doesn't exist" in captured.err

    def test_init_from_config_file(self, operators_config_file):
        conf = Config(operators_config_file)
        assert conf.config_file == operators_config_file
        assert conf.prefix == ""

    def test_init_from_config_file_and_prefix(self, operators_config_file):
        conf = Config(operators_config_file, prefix="abcd")
        assert conf.config_file == operators_config_file
        assert conf.prefix == "abcd"

    def test_dynamic_attribute_not_allowed(self, operators_config_file):
        obj = Config(operators_config_file)
        with pytest.raises(AttributeError):
            obj.custom_attribute = 5


class TestFragment:
    def test_init(self):
        f = Fragment()
        assert f.name == ""

    def test_init_with_wrong_args(self):
        with pytest.raises(ValueError, match=r".*must be the Application.*"):
            Fragment(Fragment())

    def test_name(self, fragment):
        fragment.name = "fragment_1"
        assert fragment.name == "fragment_1"

        with pytest.raises(TypeError):
            fragment.name = 5

    def test_application(self, fragment):
        app = Application()
        fragment.application = app

    def test_graph(self, fragment):
        # first call to fragment.graph constructs a FlowGraph object
        graph = fragment.graph
        assert isinstance(graph, OperatorGraph)
        assert isinstance(graph, OperatorFlowGraph)
        assert not isinstance(graph, FragmentGraph)
        # former Graph and FlowGraph names also still present
        assert isinstance(graph, Graph)
        assert isinstance(graph, FlowGraph)

    def test_executor(self, fragment):
        # first call to fragment.graph constructs a FlowGraph object
        executor = fragment.executor
        assert isinstance(executor, Executor)
        assert isinstance(executor, GXFExecutor)
        assert executor.fragment is fragment

    def test_from_config(self, fragment, operators_config_file):
        fragment.config(operators_config_file)

        replayer_kwargs = fragment.from_config("replayer")
        assert isinstance(replayer_kwargs, ArgList)
        assert replayer_kwargs.size == len(replayer_kwargs.args) == 5
        # all arguments in the ArgList are YAML nodes
        for arg in replayer_kwargs.args:
            assert arg.arg_type.element_type == ArgElementType.YAML_NODE

    def test_from_config_nested_key(self, fragment, operators_config_file):
        fragment.config(operators_config_file)

        width = fragment.from_config("replayer.frame_rate")
        assert isinstance(width, Arg)
        assert width.arg_type.element_type == ArgElementType.YAML_NODE

    def test_from_config_missing_key(self, fragment, operators_config_file, capfd):
        fragment.config(operators_config_file)
        nonexistent_kwargs = fragment.from_config("nonexistent")
        assert nonexistent_kwargs.size == 0
        msg = "Unable to find the parameter item/map with key 'nonexistent'"
        captured = capfd.readouterr()
        assert "error" in captured.err
        assert msg in captured.err

    def test_uninitialized_config(self, fragment):
        assert fragment.config().config_file == ""

    def test_add_operator(self, fragment):
        op_tx, op_rx = get_tx_and_rx_ops(fragment)
        fragment.add_operator(op_tx)
        fragment.add_operator(op_rx)

    def test_make_thread_pool(self, fragment):
        op_tx, op_rx = get_tx_and_rx_ops(fragment)
        op_tx2, op_rx2 = get_tx_and_rx_ops(fragment)

        pool1 = fragment.make_thread_pool("pool1", 2)
        pool1.add(op_tx, True)
        pool1.add(op_rx, False)

        pool2 = fragment.make_thread_pool("pool2", 2)
        pool2.add([op_tx2, op_rx2], True)

        assert pool1.name == "pool1"
        assert pool2.name == "pool2"

        # check that the expected operators are associated with each pool
        assert op_rx in pool1.operators
        assert op_tx in pool1.operators
        assert op_rx2 in pool2.operators
        assert op_tx2 in pool2.operators

        assert "gxf_typename: nvidia::gxf::ThreadPool" in repr(pool1)
        assert "operators in pool" in repr(pool1)

    def test_add_flow(self, fragment, capfd):
        op_tx, op_rx = get_tx_and_rx_ops(fragment)

        # test add_flow with tuple of 2-tuple
        fragment.add_flow(op_tx, op_rx, {("tensor", "in1")})
        # test add_flow with set of 2-tuple
        fragment.add_flow(op_tx, op_rx, {("tensor", "in2")})

        # using non-existent names doesn't yet raise a Python exception...
        fragment.add_flow(op_tx, op_rx, {("nonexistent", "in2")})

        captured = capfd.readouterr()
        assert "error" in captured.err
        assert "nonexistent" in captured.err

    def test_dynamic_attribute_allowed(self, fragment):
        fragment.custom_attribute = 5


class TestApplication:
    def test_init(self):
        import sys

        # Note that executing the following code (Application()) under PyTest can cause a segfault
        # if Application.__init__() in python/holoscan/core/__init__.py raises an exception before
        # calling '_Application.__init__(self, *args, **kwargs)',
        # triggering Application.__repr__() to be called, which in turn calls
        # Application::name() when the Application object is not fully initialized.
        #
        #   .def(
        #       "__repr__",
        #       [](const Application& app) {
        #         return fmt::format("<holoscan.Application: name:{}>", app.name());
        #       },
        #       py::call_guard<py::gil_scoped_release>(),
        #       R"doc(Return repr(self).)doc");
        #
        # Please make sure that no exceptions are raised before calling '_Application.__init__()' in
        # Application.__init__() in python/holoscan/core/__init__.py.
        #
        # This issue was handled by taking the pointer to the Application (not the reference) and
        # checking if it is nullptr before calling Application::name() in Application.__repr__().
        app = Application()
        assert isinstance(app, _Fragment)
        assert len(app.argv) > 0
        assert app.argv == sys.argv

        app = Application([sys.executable, *sys.argv])
        assert len(app.argv) > 0
        assert app.argv == sys.argv
        assert repr(CLIOptions()) == repr(app.options)

    def test_init_with_argv(self):
        import sys

        app = Application([])
        assert app.argv == sys.argv

        app = Application([sys.executable])
        assert app.argv == [""]

        app = Application(
            [
                sys.executable,
                "app.py",
                "arg1",
                "--driver",
                "--option1=value1",
                "arg2",
                "--option2",
                "value2",
                "--worker",
                "arg3",
                "--address",
                "10.0.0.1",
                "arg4",
                "--worker-address=10.0.0.2",
                "arg5",
                "--fragments=fragment_1",
                "arg6",
                "--config=config.yaml",
                "arg7",
                "--fragments",
                "fragment_2,fragment_3",
            ]
        )
        assert app.argv == [
            "app.py",
            "arg1",
            "--option1=value1",
            "arg2",
            "--option2",
            "value2",
            "arg3",
            "arg4",
            "arg5",
            "arg6",
            "arg7",
        ]
        assert repr(
            CLIOptions(
                run_driver=True,
                run_worker=True,
                driver_address="10.0.0.1",
                worker_address="10.0.0.2",
                worker_targets=["fragment_1", "fragment_2", "fragment_3"],
                config_path="config.yaml",
            )
        ) == repr(app.options)

    def test_name(self, app):
        app.name = "app_1"
        assert app.name == "app_1"

        with pytest.raises(TypeError):
            app.name = 5

    def test_description(self, app):
        app.description = "my app description"
        assert app.description == "my app description"

        with pytest.raises(TypeError):
            app.description = 5

    def test_version(self, app):
        app.version = "1.2.3"
        assert app.version == "1.2.3"

        with pytest.raises(TypeError):
            app.version = 5

    def test_options(self, app):
        assert app.options.run_driver is False
        assert app.options.run_worker is False
        assert app.options.driver_address == ""
        assert app.options.worker_address == ""
        assert app.options.worker_targets == []
        assert app.options.config_path == ""

        with pytest.raises(AttributeError):
            app.options = 3

    def test_graph(self, app):
        # first call to app.graph constructs a FlowGraph object
        graph = app.graph
        assert isinstance(graph, Graph)
        assert isinstance(graph, FlowGraph)

    def test_executor(self, app):
        # first call to app.graph constructs a FlowGraph object
        executor = app.executor
        assert isinstance(executor, Executor)
        assert isinstance(executor, GXFExecutor)
        assert executor.fragment is app

    def test_from_config(self, app, operators_config_file):
        app.config(operators_config_file)

        replayer_kwargs = app.from_config("replayer")
        assert isinstance(replayer_kwargs, ArgList)
        assert replayer_kwargs.size == 5

    def test_kwargs(self, app, operators_config_file):
        app.config(operators_config_file)

        replayer_kwargs = app.kwargs("replayer")
        assert isinstance(replayer_kwargs, dict)
        assert "frame_rate" in replayer_kwargs

    def test_from_config_missing_key(self, app, operators_config_file, capfd):
        app.config(operators_config_file)
        nonexistent_kwargs = app.from_config("nonexistent")
        assert nonexistent_kwargs.size == 0
        msg = "Unable to find the parameter item/map with key 'nonexistent'"
        captured = capfd.readouterr()
        assert "error" in captured.err
        assert msg in captured.err

    def test_uninitialized_config(self, app):
        assert app.config().config_file == ""

    def test_add_operator(self, app):
        op_tx, op_rx = get_tx_and_rx_ops(app)
        app.add_operator(op_tx)
        app.add_operator(op_rx)

    def test_add_fragment(self, app):
        fragment1 = Fragment(app, name="fragment1")
        fragment2 = Fragment(app, name="fragment2")

        app.add_fragment(fragment1)
        app.add_fragment(fragment2)

    def test_reserved_fragment_name(self, app, capfd):
        Fragment(app, name="all")

        captured = capfd.readouterr()
        assert "Fragment name 'all' is reserved" in captured.err

    def test_add_flow(self, app, capfd):
        op_tx, op_rx = get_tx_and_rx_ops(app)

        # list of 2-tuples
        with pytest.raises(TypeError):
            app.add_flow(op_tx, op_rx, [("tensor", "in1")])
        # set of 2-tuples
        app.add_flow(op_tx, op_rx, {("tensor", "in2")})

        # using non-existent names doesn't yet raise a Python exception...
        app.add_flow(op_tx, op_rx, {("nonexistent", "in2")})

        captured = capfd.readouterr()
        assert "error" in captured.err
        assert "nonexistent" in captured.err

    def test_add_operators_with_same_name(self, app, capfd):
        op_tx = OpTx(app, name="op")
        op_rx = OpRx(app, name="op")

        app.add_operator(op_tx)
        with pytest.raises(RuntimeError):
            app.add_operator(op_rx)

        with pytest.raises(RuntimeError):
            app.add_flow(op_tx, op_rx, {("tensor", "in2")})

        captured = capfd.readouterr()
        assert "duplicate name" in captured.err

    def test_add_flow_fragments(self, app, capfd):
        fragment1 = Fragment(app, "fragment1")
        fragment2 = Fragment(app, "fragment2")

        # list of 2-tuples
        with pytest.raises(TypeError):
            app.add_flow(fragment1, fragment2, [("blur_image", "sharpen_image")])
        # set of 2-tuples
        app.add_flow(fragment1, fragment2, {("blur_image", "sharpen_image")})

        # using empty dict (should raise a TypeError because port_pairs accept Set[Tuple[str,str]])
        with pytest.raises(TypeError):
            app.add_flow(fragment1, fragment2, {})

        # using empty set shows an error message ('Unable to add fragment flow with empty
        # port_pairs')
        app.add_flow(fragment1, fragment2, set())

        captured = capfd.readouterr()
        assert "error" in captured.err
        assert "empty port_pairs" in captured.err

    def test_add_fragments_with_same_name(self, app, capfd):
        fragment1 = Fragment(app, "fragment")
        fragment2 = Fragment(app, "fragment")

        app.add_fragment(fragment1)
        with pytest.raises(RuntimeError):
            app.add_fragment(fragment2)

        with pytest.raises(RuntimeError):
            app.add_flow(fragment1, fragment2, {("blur_image", "sharpen_image")})

        captured = capfd.readouterr()
        assert "duplicate name" in captured.err

    def test_dynamic_attribute(self, app):
        # verify that attributes not in the underlying C++ class can be
        # dynamically added
        app.custom_attribute = 5


class TestIOSpec:
    @pytest.mark.parametrize(
        "name, io_type",
        [("input", IOSpec.IOType.INPUT), ("output", IOSpec.IOType.OUTPUT)],
    )
    def test_init(self, fragment, name, io_type):
        op_spec = OperatorSpecBase(fragment)
        io_spec = IOSpec(op_spec, name, io_type)
        assert io_spec.name == name
        assert io_spec.io_type == io_type
        assert io_spec.connector() is None
        assert io_spec.conditions == []
        assert io_spec.queue_size == 1

        io_spec.queue_size = 5
        assert io_spec.queue_size == 5
        io_spec.queue_size = int(IOSpec.SIZE_ONE)

        repr_str = repr(io_spec)
        if name == "output":
            assert "io_type: kOutput" in repr_str
        elif name == "input":
            assert "io_type: kInput" in repr_str
        assert f"name: {io_spec.name}" in repr_str
        assert "connector_type: kDefault" in repr_str

    @pytest.mark.parametrize(
        "name, io_type",
        [("input", IOSpec.IOType.INPUT), ("output", IOSpec.IOType.OUTPUT)],
    )
    def test_kwarg_init(self, fragment, name, io_type):
        op_spec = OperatorSpecBase(fragment)
        io_spec = IOSpec(op_spec=op_spec, name=name, io_type=io_type)
        assert io_spec.name == name
        assert io_spec.io_type == io_type

    def test_dynamic_attribute_not_allowed(self, fragment):
        op_spec = OperatorSpecBase(fragment)
        io_spec = IOSpec(op_spec=op_spec, name="in", io_type=IOSpec.IOType.INPUT)
        with pytest.raises(AttributeError):
            io_spec.custom_attribute = 5


class TestExecutor:
    def test_init(self, fragment):
        e = Executor(fragment)
        assert e.context is None

    def test_dynamic_attribute_not_allowed(self, fragment):
        obj = Executor(fragment)
        with pytest.raises(AttributeError):
            obj.custom_attribute = 5


class TestScheduler:
    def test_base_class_init(self):
        s = Scheduler(name="greedy")
        assert s.name == "greedy"
        assert len(s.args) == 0
        # id and fragment will not yet have been set
        assert s.id == -1
        assert s.fragment is None


class TestNetworkContext:
    def test_base_class_init(self):
        s = NetworkContext(name="network")
        assert s.name == "network"
        assert len(s.args) == 0
        # id and fragment will not yet have been set
        assert s.id == -1
        assert s.fragment is None


class TestAsTensor:
    @pytest.mark.parametrize("use_cupy", [False, True])
    def test_as_tensor_stride_workaround(self, use_cupy):
        # Due to a bug in CuPy<13.0.0a1, it is possible for cupy.astype or cupy.asarray
        # to convert an array that is both C and F contiguous into Fortran-style strides
        # which is inconsistent with NumPy behavior. We force a copy of such an array that
        # is both C and F contiguous to always have C-contiguous strides so that the
        # as_tensor behavior is consistent regardless of the CuPy version.
        xp = pytest.importorskip("cupy" if use_cupy else "numpy")

        coords = xp.array([0.1, 0.1, 0.1], dtype=xp.float32).reshape((1, 1, 3))
        assert coords.strides == (12, 12, 4)

        coords_f = coords.astype(xp.float32, order="F")
        assert coords_f.strides == (4, 4, 4)

        tensor = as_tensor(coords)
        assert tensor.strides == coords.strides

        # an array that is both C and F contiguous will become tensor with C contiguous strides
        assert coords_f.flags.c_contiguous
        assert coords_f.flags.f_contiguous

        tensor_f = as_tensor(coords_f)
        assert tensor_f.strides == coords.strides


class TestIOTypeRegistry:
    def test_registery_entries(self):
        registered_types = io_type_registry.registered_types()

        # not an exhaustive list, just a few examples
        assert "holoscan::Tensor" in registered_types
        assert "CloudPickleSerializedObject" in registered_types
        assert "std::string" in registered_types
        assert "PyObject" in registered_types

    def test_holoviz_registered_types(self):
        # import HolovizOp to ensure its associated types will have been registered
        from holoscan.operators import HolovizOp  # noqa: F401

        registered_types = io_type_registry.registered_types()
        assert "std::shared_ptr<nvidia::gxf::Pose3D>" in registered_types
        assert "std::shared_ptr<std::array<float, 16>>" in registered_types
        assert "std::vector<HolovizOp::InputSpec>" in registered_types
