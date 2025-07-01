/*
 * SPDX-FileCopyrightText: Copyright (c) 2023-2025 NVIDIA CORPORATION & AFFILIATES. All rights reserved.
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

#ifndef PYHOLOSCAN_CORE_COMPONENT_PYDOC_HPP
#define PYHOLOSCAN_CORE_COMPONENT_PYDOC_HPP

#include <string>

#include "../macros.hpp"

namespace holoscan::doc {

namespace ParameterFlag {

//  Constructor
PYDOC(ParameterFlag, R"doc(
Enum class for parameter flags.

The following flags are supported:
- `NONE`: The parameter is mendatory and static. It cannot be changed at runtime.
- `OPTIONAL`: The parameter is optional and might not be available at runtime.
- `DYNAMIC`: The parameter is dynamic and might change at runtime.
)doc")

}  // namespace ParameterFlag

namespace ComponentSpec {

//  Constructor
PYDOC(ComponentSpec, R"doc(
Component specification class.

Parameters
----------
fragment : holoscan.core.Fragment
    The fragment that the component belongs to.
)doc")

PYDOC(fragment, R"doc(
The fragment that the component belongs to.

Returns
-------
name : holoscan.core.Fragment
)doc")

PYDOC(params, R"doc(
The parameters associated with the component.
)doc")

PYDOC(description, R"doc(
YAML formatted string describing the component spec.
)doc")

PYDOC(param, R"doc(
Add a parameter to the specification.

Parameters
----------
param : name
    The name of the parameter.
default_value : object
    The default value for the parameter.

Additional Parameters
---------------------
headline : str, optional
    If provided, this is a brief "headline" description for the parameter.
description : str, optional
    If provided, this is a description for the parameter (typically more verbose than the brief
    description provided via `headline`).
kind : str, optional
    In most cases, this keyword should not be specified. If specified, the only valid option is
    currently ``kind="receivers"``, which can be used to create a parameter holding a vector of
    receivers. This effectively creates a multi-receiver input port to which any number of
    operators can be connected.
    Since Holoscan SDK v2.3, users can define a multi-receiver input port using `spec.input()` with
    `size=IOSpec.ANY_SIZE`, instead of using `spec.param()` with `kind="receivers"`. It is now
    recommended to use this new `spec.input`-based approach and the old "receivers" parameter
    approach should be considered deprecated.
flag: holoscan.core.ParameterFlag, optional
    If provided, this is a flag that can be used to control the behavior of the parameter.
    By default, `ParameterFlag.NONE` is used.

    The following flags are supported:
    - `ParameterFlag.NONE`: The parameter is mendatory and static. It cannot be changed at runtime.
    - `ParameterFlag.OPTIONAL`: The parameter is optional and might not be available at runtime.
    - `ParameterFlag.DYNAMIC`: The parameter is dynamic and might change at runtime.

Notes
-----
This method is intended to be called within the `setup` method of a Component, Condition or
Resource.

In general, for native Python resources, it is not necessary to call `param` to register a
parameter with the class. Instead, one can just directly add parameters to the Python resource
class (e.g. For example, directly assigning ``self.param_name = value`` in __init__.py).
)doc")

}  // namespace ComponentSpec

namespace Component {

//  Constructor
PYDOC(Component, R"doc(
Base component class.
)doc")

PYDOC(name, R"doc(
The name of the component.

Returns
-------
name : str
)doc")

PYDOC(fragment, R"doc(
The fragment containing the component.

Returns
-------
name : holoscan.core.Fragment
)doc")

PYDOC(id, R"doc(
The identifier of the component.

The identifier is initially set to ``-1``, and will become a valid value when the
component is initialized.

With the default executor (`holoscan.gxf.GXFExecutor`), the identifier is set to the GXF
component ID.

Returns
-------
id : int
)doc")

PYDOC(add_arg_Arg, R"doc(
Add an argument to the component.
)doc")

PYDOC(add_arg_ArgList, R"doc(
Add a list of arguments to the component.
)doc")

PYDOC(args, R"doc(
The list of arguments associated with the component.

Returns
-------
arglist : holoscan.core.ArgList
)doc")

PYDOC(initialize, R"doc(
Initialize the component.
)doc")

PYDOC(description, R"doc(
YAML formatted string describing the component.
)doc")

PYDOC(service, R"doc(
Retrieve a registered fragment service through the component's fragment.

This method delegates to the fragment's service() method to retrieve a previously
registered fragment service by its type and optional identifier.
Returns ``None`` if no fragment service is found with the specified type and identifier.

Parameters
----------
service_type : type
    The type of the fragment service to retrieve. Must be a type that inherits from
    Resource or FragmentService.
id : str, optional
    The identifier of the fragment service. If empty, retrieves by service type only.
    For Resources, this would typically be the resource's name.

Returns
-------
object or None
    The fragment service instance of the requested type, or ``None`` if not found.
    If the service wraps a Resource and a Resource type is requested, the unwrapped
    Resource instance is returned.

Raises
------
RuntimeError
    If the component has no associated fragment or if the fragment's service method
    cannot be accessed.

Notes
-----
This is a convenience method that internally calls the fragment's service() method.
For services that wrap Resources, the method will automatically unwrap and return
the Resource if a Resource type is requested.
)doc")

}  // namespace Component

}  // namespace holoscan::doc

#endif  // PYHOLOSCAN_CORE_COMPONENT_PYDOC_HPP
