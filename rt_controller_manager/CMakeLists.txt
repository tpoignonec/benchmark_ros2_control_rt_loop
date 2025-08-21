cmake_minimum_required(VERSION 3.8)
project(rt_controller_manager)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)

set(DEPENDENCIES
  rt_diagnostics_msgs
  controller_manager
  pluginlib
  rclcpp
  realtime_tools
  std_msgs
)

find_package(backward_ros REQUIRED)
foreach(Dependency IN ITEMS ${DEPENDENCIES})
  find_package(${Dependency} REQUIRED)
endforeach()

add_executable(ros2_control_node src/ros2_control_node.cpp)
target_include_directories(ros2_control_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)
target_compile_features(ros2_control_node PUBLIC c_std_99 cxx_std_17)  # Require C99 and C++17
ament_target_dependencies(ros2_control_node ${DEPENDENCIES})
install(TARGETS ros2_control_node
  DESTINATION lib/${PROJECT_NAME})

if(BUILD_TESTING)
  find_package(ament_lint_auto REQUIRED)
  # the following line skips the linter which checks for copyrights
  # comment the line when a copyright and license is added to all source files
  set(ament_cmake_copyright_FOUND TRUE)
  # the following line skips cpplint (only works in a git repo)
  # comment the line when this package is in a git repo and when
  # a copyright and license is added to all source files
  set(ament_cmake_cpplint_FOUND TRUE)
  ament_lint_auto_find_test_dependencies()
endif()

ament_package()
