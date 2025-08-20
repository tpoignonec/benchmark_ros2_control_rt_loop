# benchmark_ros2_control_rt_loop
Little benchmark to test RT-compatibility of ros2_control


# Installation

1) Prepare the ros2 workspace

```bash
# Go to/create ros2 workspace dir <ws>
export ROS2_WS=~/dev/src/ws_benchmark_ros2_control
mkdir -p $ROS2_WS/src
cd $ROS2_WS/src

# Clone this repos
git clone https://github.com/tpoignonec/benchmark_ros2_control_rt_loop.git

# Clone dependencies
vcs import . < benchmark_ros2_control_rt_loop/benchmark_ros2_control_rt_loop.repos

cd ..
```

```bash
# Source ros2 distro
source /opt/ros/jazzy/setup.bash

# Install dependencies
rosdep install --ignore-src --from-paths . -y -r
```

2) Build

```bash
colcon build --cmake-args -DCMAKE_BUILD_TYPE=Release
```