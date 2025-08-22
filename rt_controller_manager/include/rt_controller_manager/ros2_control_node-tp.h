#undef TRACEPOINT_PROVIDER
#define TRACEPOINT_PROVIDER ros2_control_node

#undef TRACEPOINT_INCLUDE
#define TRACEPOINT_INCLUDE "rt_controller_manager/ros2_control_node-tp.h"

#if !defined(_ROS2_CONTROL_NODE_TP_H) || defined(TRACEPOINT_HEADER_MULTI_READ)
#define _ROS2_CONTROL_NODE_TP_H

#include <lttng/tracepoint.h>

// Function start tracepoint: func_start
LTTNG_UST_TRACEPOINT_EVENT(
    TRACEPOINT_PROVIDER,
    func_start,
    TP_ARGS(
        const char*, func_name
    ),
    TP_FIELDS(
        ctf_string(func, func_name)
    )
)

// End tracepoint: func_end
LTTNG_UST_TRACEPOINT_EVENT(
    TRACEPOINT_PROVIDER,
    func_end,
    TP_ARGS(
        const char*, func_name
    ),
    TP_FIELDS(
        ctf_string(func, func_name)
    )
)

#endif // RT_CONTROLLER_MANAGER_ROS2_CONTROL_NODE_TP_HPP

#include <lttng/tracepoint-event.h>
