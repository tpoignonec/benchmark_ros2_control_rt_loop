// Copyright 2020 ROS2-Control Development Team
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <errno.h>
#include <chrono>
#include <memory>
#include <string>
#include <thread>

#include "controller_manager/controller_manager.hpp"
#include "rclcpp/executors.hpp"
#include "realtime_tools/realtime_helpers.hpp"
#include "realtime_tools/realtime_publisher.hpp"

#include "rt_diagnostics_msgs/msg/call_stats.hpp"
#include "rt_diagnostics_msgs/msg/call_stats_array.hpp"

using namespace std::chrono_literals;

namespace
{
// Reference: https://man7.org/linux/man-pages/man2/sched_setparam.2.html
// This value is used when configuring the main loop to use SCHED_FIFO scheduling
// We use a midpoint RT priority to allow maximum flexibility to users
int const kSchedPriority = 50;

/**
 * @brief A utility class for measuring timing statistics of function calls in real-time control loops.
 *
 * This class provides functionality to wrap controller manager functions (read, update, write)
 * and collect timing statistics including execution duration and period between calls.
 * It's designed for use in real-time control systems where performance monitoring is critical.
 */
class FunctionCallStats
{
public:
  FunctionCallStats()
  : previous_time_{0, 0}, measured_period_{0, 0}, measured_duration_{0, 0} {}

  /**
   * @brief Creates a wrapper function that measures timing statistics around the provided function.
   *
   * This template method wraps any function that matches the controller manager signature
   * (const rclcpp::Time&, const rclcpp::Duration&) and automatically measures:
   * - Execution duration: time taken to execute the wrapped function
   * - Call period: time elapsed between consecutive calls to the wrapper
   *
   * @tparam Func The type of the function to be wrapped
   * @param cm Shared pointer to the controller manager (used for clock access)
   * @param func The function to wrap and measure
   * @return A lambda function that can be called with the same signature as the original function
   *
   * @note The wrapper uses the controller manager's trigger clock for consistent timing measurements.
   * @note The first call will have a period of 0 since there's no previous call to compare against.
   */
  template<typename Func>
  auto create_wrapper(std::shared_ptr<controller_manager::ControllerManager> cm, Func func)
  {
    return [this, cm, func](const rclcpp::Time& time, const rclcpp::Duration& period)
    {
      auto const start_time = cm->get_trigger_clock()->now();

      // Call the actual function (read, update, or write)
      func(time, period);

      auto const end_time = cm->get_trigger_clock()->now();
      measured_duration_ = end_time - start_time;

      // Calculate the measured period
      if (previous_time_.nanoseconds() == 0)
      {
        // If this is the first call, we cannot calculate a period
        previous_time_ = start_time;
        measured_period_ = rclcpp::Duration(0, 0);
      }
      else
      {
        measured_period_ = start_time - previous_time_;
      }
      // Update the previous time
      previous_time_ = time;
    };
  }

  rclcpp::Time get_call_time() const { return previous_time_; }
  rclcpp::Duration get_period() const { return measured_period_; }
  rclcpp::Duration get_duration() const { return measured_duration_; }

  /**
   * @brief Converts the collected statistics into a CallStats message.
   *
   * This method populates a CallStats message with the timing statistics collected
   * during the function calls.
   *
   * @return A populated rt_diagnostics_msgs::msg::CallStats message
   */
  rt_diagnostics_msgs::msg::CallStats to_call_stats_msg() const
  {
    rt_diagnostics_msgs::msg::CallStats stats_msg;
    stats_msg.call_time = previous_time_;
    stats_msg.time_since_last = measured_period_;
    stats_msg.call_duration = measured_duration_;
    return stats_msg;
  }

private:
  rclcpp::Time previous_time_;  ///< Timestamp of the previous function call start
  rclcpp::Duration measured_period_;  ///< Time between consecutive calls
  rclcpp::Duration measured_duration_;  ///< Execution time of the last function call
};

}  // namespace

int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);

  std::shared_ptr<rclcpp::Executor> executor =
    std::make_shared<rclcpp::executors::MultiThreadedExecutor>();
  std::string manager_node_name = "controller_manager";

  rclcpp::NodeOptions cm_node_options = controller_manager::get_cm_node_options();
  std::vector<std::string> node_arguments = cm_node_options.arguments();
  for (int i = 1; i < argc; ++i)
  {
    if (node_arguments.empty() && std::string(argv[i]) != "--ros-args")
    {
      // A simple way to reject non ros args
      continue;
    }
    node_arguments.push_back(argv[i]);
  }
  cm_node_options.arguments(node_arguments);

  auto cm = std::make_shared<controller_manager::ControllerManager>(
    executor, manager_node_name, "", cm_node_options);

  const bool use_sim_time = cm->get_parameter_or("use_sim_time", false);

  const bool has_realtime = realtime_tools::has_realtime_kernel();
  const bool lock_memory = cm->get_parameter_or<bool>("lock_memory", has_realtime);
  if (lock_memory)
  {
    const auto lock_result = realtime_tools::lock_memory();
    if (!lock_result.first)
    {
      RCLCPP_WARN(cm->get_logger(), "Unable to lock the memory: '%s'", lock_result.second.c_str());
    }
  }

  rclcpp::Parameter cpu_affinity_param;
  if (cm->get_parameter("cpu_affinity", cpu_affinity_param))
  {
    std::vector<int> cpus = {};
    if (cpu_affinity_param.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER)
    {
      cpus = {static_cast<int>(cpu_affinity_param.as_int())};
    }
    else if (cpu_affinity_param.get_type() == rclcpp::ParameterType::PARAMETER_INTEGER_ARRAY)
    {
      const auto cpu_affinity_param_array = cpu_affinity_param.as_integer_array();
      std::for_each(
        cpu_affinity_param_array.begin(), cpu_affinity_param_array.end(),
        [&cpus](int cpu) { cpus.push_back(static_cast<int>(cpu)); });
    }
    const auto affinity_result = realtime_tools::set_current_thread_affinity(cpus);
    if (!affinity_result.first)
    {
      RCLCPP_WARN(
        cm->get_logger(), "Unable to set the CPU affinity : '%s'", affinity_result.second.c_str());
    }
  }

  // wait for the clock to be available
  cm->get_clock()->wait_until_started();
  cm->get_clock()->sleep_for(rclcpp::Duration::from_seconds(1.0 / cm->get_update_rate()));

  RCLCPP_INFO(cm->get_logger(), "update rate is %d Hz", cm->get_update_rate());
  const int thread_priority = cm->get_parameter_or<int>("thread_priority", kSchedPriority);
  RCLCPP_INFO(
    cm->get_logger(), "Spawning %s RT thread with scheduler priority: %d", cm->get_name(),
    thread_priority);

  std::thread cm_thread(
    [cm, thread_priority, use_sim_time]()
    {
      if (!realtime_tools::configure_sched_fifo(thread_priority))
      {
        RCLCPP_WARN(
          cm->get_logger(),
          "Could not enable FIFO RT scheduling policy: with error number <%i>(%s). See "
          "[https://control.ros.org/master/doc/ros2_control/controller_manager/doc/userdoc.html] "
          "for details on how to enable realtime scheduling.",
          errno, strerror(errno));
      }
      else
      {
        RCLCPP_INFO(
          cm->get_logger(), "Successful set up FIFO RT scheduling policy with priority %i.",
          thread_priority);
      }

      // for calculating sleep time
      auto const period = std::chrono::nanoseconds(1'000'000'000 / cm->get_update_rate());

      // for calculating the measured period of the loop
      rclcpp::Time previous_time = cm->get_trigger_clock()->now();
      std::this_thread::sleep_for(period);

      std::chrono::steady_clock::time_point next_iteration_time{std::chrono::steady_clock::now()};


      // Wrap the controller manager functions to measure their execution time
      FunctionCallStats read_stats;
      FunctionCallStats update_stats;
      FunctionCallStats write_stats;

      auto read_wrapper = read_stats.create_wrapper(
        cm, [&cm](const rclcpp::Time& time, const rclcpp::Duration& period) {
          cm->read(time, period);
      });

      auto update_wrapper = update_stats.create_wrapper(
        cm, [&cm](const rclcpp::Time& time, const rclcpp::Duration& period) {
          cm->update(time, period);
      });

      auto write_wrapper = write_stats.create_wrapper(
        cm, [&cm](const rclcpp::Time& time, const rclcpp::Duration& period) {
          cm->write(time, period);
      });

      // Add RT publisher for RW statistics
      auto rw_stats_publisher = cm->create_publisher<rt_diagnostics_msgs::msg::CallStatsArray>(
        "~/call_stats", rclcpp::SystemDefaultsQoS());
      auto realtime_rw_stats_publisher = std::make_unique<realtime_tools::RealtimePublisher<
        rt_diagnostics_msgs::msg::CallStatsArray>>(rw_stats_publisher);

      rt_diagnostics_msgs::msg::CallStatsArray rw_stats_msg;
      rw_stats_msg.function_names = {
        "read", "update", "write"
      };
      rw_stats_msg.stats.resize(3);

      while (rclcpp::ok())
      {
        // calculate measured period
        auto const current_time = cm->get_trigger_clock()->now();
        auto const measured_period = current_time - previous_time;
        previous_time = current_time;

        // execute update loop with wrapped functions
        read_wrapper(cm->get_trigger_clock()->now(), measured_period);
        update_wrapper(cm->get_trigger_clock()->now(), measured_period);
        write_wrapper(cm->get_trigger_clock()->now(), measured_period);

        // collect statistics
        rw_stats_msg.header.stamp = current_time;
        rw_stats_msg.stats[0] = read_stats.to_call_stats_msg();
        rw_stats_msg.stats[1] = update_stats.to_call_stats_msg();
        rw_stats_msg.stats[2] = write_stats.to_call_stats_msg();

        // publish statistics if the publisher is ready
        if (realtime_rw_stats_publisher->trylock())
        {
          realtime_rw_stats_publisher->msg_ = rw_stats_msg;
          realtime_rw_stats_publisher->unlockAndPublish();
        }
        else
        {
          RCLCPP_WARN(
            cm->get_logger(), "Could not publish call statistics, publisher is not ready.");
        }

        // wait until we hit the end of the period
        if (use_sim_time)
        {
          cm->get_clock()->sleep_until(current_time + period);
        }
        else
        {
          next_iteration_time += period;
          const auto time_now = std::chrono::steady_clock::now();
          if (next_iteration_time < time_now)
          {
            const double time_diff =
              static_cast<double>(
                std::chrono::duration_cast<std::chrono::nanoseconds>(time_now - next_iteration_time)
                  .count()) /
              1.e6;
            const double cm_period = 1.e3 / static_cast<double>(cm->get_update_rate());
            const int overrun_count = static_cast<int>(std::ceil(time_diff / cm_period));
            RCLCPP_WARN_THROTTLE(
              cm->get_logger(), *cm->get_clock(), 1000,
              "Overrun detected! The controller manager missed its desired rate of %d Hz. The loop "
              "took %f ms (missed cycles : %d).",
              cm->get_update_rate(), time_diff + cm_period, overrun_count + 1);
            next_iteration_time += (overrun_count * period);
          }
          std::this_thread::sleep_until(next_iteration_time);
        }
      }
    });

  executor->add_node(cm);
  executor->spin();
  cm_thread.join();
  rclcpp::shutdown();
  return 0;
}
