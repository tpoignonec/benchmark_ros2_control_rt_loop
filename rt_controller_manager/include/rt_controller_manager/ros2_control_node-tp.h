#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER ros2_control_node

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "rt_controller_manager/ros2_control_node-tp.h"

#if !defined(_ROS2_CONTROL_NODE_TP_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _ROS2_CONTROL_NODE_TP_H

#include <lttng/tracepoint.h>

LTTNG_UST_TRACEPOINT_EVENT(
    TRACEPOINT_PROVIDER,
    function_call,
    TP_ARGS(
        const char*, func_name,
        uint64_t, timestamp_ns,
        uint64_t, duration_ns
    ),
    TP_FIELDS(
        lttng_ust_field_string(function_name, func_name)
        lttng_ust_field_integer(uint64_t, timestamp, timestamp_ns)
        lttng_ust_field_integer(uint64_t, duration, duration_ns)
    )
)

// Macro to trace function calls with timing
#define TRACE_FUNCTION_CALL(func_name, func_call, clock) \
{ \
    auto tmp_call_start = (clock)->now(); \
    func_call; \
    lttng_ust_tracepoint( \
        ros2_control_node, function_call, func_name, \
        static_cast<uint64_t>(tmp_call_start.nanoseconds()), \
        static_cast<uint64_t>(((clock)->now() - tmp_call_start).nanoseconds())); \
}

#endif // RT_CONTROLLER_MANAGER_ROS2_CONTROL_NODE_TP_HPP

#include <lttng/tracepoint-event.h>
