cmake_minimum_required(VERSION 3.8)
project(rt_controller_manager)

if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# find dependencies
find_package(ament_cmake REQUIRED)

option(TRACETOOLS_DISABLED "Explicitly disable support for tracing" ${DISABLED_DEFAULT})

set(DEPENDENCIES
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

# Build LTTng tracepoints

add_library(${PROJECT_NAME}_tracepoints src/ros2_control_node-tp.cpp)
if(NOT TRACETOOLS_DISABLED)
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(LTTNG REQUIRED lttng-ust)
  target_link_libraries(${PROJECT_NAME}_tracepoints PRIVATE
    ${LTTNG_LIBRARIES}
    rcpputils::rcpputils
  )
endif()

target_include_directories(${PROJECT_NAME}_tracepoints PUBLIC
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
)
if(TRACETOOLS_DISABLED)
  target_compile_definitions(${PROJECT_NAME}_tracepoints PRIVATE TRACETOOLS_DISABLED)
endif()

install(
  DIRECTORY include/
  DESTINATION include/${PROJECT_NAME}
)
install(
  TARGETS ${PROJECT_NAME}_tracepoints
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  RUNTIME DESTINATION bin
)

# Build ros2_control_node

add_executable(ros2_control_node src/ros2_control_node.cpp)
target_include_directories(ros2_control_node PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include/${PROJECT_NAME}>)
target_compile_features(ros2_control_node PUBLIC c_std_99 cxx_std_17)  # Require C99 and C++17
ament_target_dependencies(ros2_control_node ${DEPENDENCIES})

# Link tracepoints to node
target_link_libraries(ros2_control_node ${PROJECT_NAME}_tracepoints)

install(
  TARGETS
    ros2_control_node
    ${PROJECT_NAME}_tracepoints
  DESTINATION
    lib/${PROJECT_NAME}
)

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
