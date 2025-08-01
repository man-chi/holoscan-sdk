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

#ifndef PYHOLOSCAN_RESOURCES_COMPONENT_SERIALIZERS_PYDOC_HPP
#define PYHOLOSCAN_RESOURCES_COMPONENT_SERIALIZERS_PYDOC_HPP

#include <string>

#include "../macros.hpp"

namespace holoscan::doc {

namespace Receiver {

PYDOC(Receiver, R"doc(
Base GXF receiver class.
)doc")

PYDOC(size, R"doc(
The size of the receiver queue's main stage.
)doc")

PYDOC(back_size, R"doc(
The size of the receiver queue's back stage.
)doc")

PYDOC(capacity, R"doc(
The capacity of the receiver queue's main stage.
)doc")

}  // namespace Receiver

namespace DoubleBufferReceiver {

PYDOC(DoubleBufferReceiver, R"doc(
Receiver using a double-buffered queue.

New messages are first pushed to a back stage.

Parameters
----------
fragment : holoscan.core.Fragment
    The fragment to assign the resource to.
capacity : int, optional
    The capacity of the receiver.
policy : int, optional
    The policy to use (0=pop, 1=reject, 2=fault).
name : str, optional
    The name of the receiver.
)doc")

}  // namespace DoubleBufferReceiver

namespace UcxReceiver {

PYDOC(UcxReceiver, R"doc(
UCX network receiver using a double-buffered queue.

New messages are first pushed to a back stage.

Parameters
----------
fragment : holoscan.core.Fragment
    The fragment to assign the resource to.
buffer : holoscan.resource.UcxSerializationBuffer
    The serialization buffer used by the transmitter.
capacity : int, optional
    The capacity of the receiver.
policy : int, optional
    The policy to use (0=pop, 1=reject, 2=fault).
address : str, optional
    The IP address used by the transmitter.
port : int, optional
    The network port used by the transmitter.
name : str, optional
    The name of the receiver.
)doc")

}  // namespace UcxReceiver

namespace AsyncBufferReceiver {

PYDOC(AsyncBufferReceiver, R"doc(
Receiver using an asynchronous buffer.

The latest message is received asynchronously.

Parameters
----------
fragment : holoscan.core.Fragment
    The fragment to assign the resource to.
name : str, optional
    The name of the receiver.
)doc")

}  // namespace AsyncBufferReceiver

}  // namespace holoscan::doc

#endif  // PYHOLOSCAN_RESOURCES_COMPONENT_SERIALIZERS_PYDOC_HPP
