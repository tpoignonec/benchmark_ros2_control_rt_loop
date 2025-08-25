# Copyright 2025 ICube Laboratory, University of Strasbourg
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

# Author: Thibault Poignonec

from launch_ros.actions import Node
from launch_ros.substitutions import FindPackageShare

from launch import LaunchDescription
from launch.substitutions import (
    Command,
    FindExecutable,
    PathJoinSubstitution
)


def generate_launch_description():  # noqa: D103
    this_package_name = 'rt_benchmark_bringup'

    config_dir = PathJoinSubstitution(
        [FindPackageShare(this_package_name), 'config']
    )

    controllers_file = PathJoinSubstitution([
        config_dir,
        'controllers.yaml'
    ])

    # Generate URDF
    robot_description_content = Command(
        [
            PathJoinSubstitution([FindExecutable(name='xacro')]),
            ' ',
            PathJoinSubstitution(
                [
                    config_dir,
                    'robot_config.xacro',
                ]
            ),
        ]
    )

    robot_description = {'robot_description': robot_description_content}

    # Start robot state publisher
    robot_state_pub_node = Node(
        package='robot_state_publisher',
        executable='robot_state_publisher',
        namespace='/',
        output='screen',
        parameters=[robot_description]
    )

    # Launch controllers
    debug_args = []  # ['--ros-args', '--log-level', 'DEBUG']

    control_node = Node(
        package='rt_controller_manager',
        executable='ros2_control_node',
        parameters=[robot_description, controllers_file],
        output='both',
        arguments=[] + debug_args,
    )

    nodes = [
        robot_state_pub_node,
        control_node
    ]

    # load_joint_state_broadcaster = ExecuteProcess(
    #     cmd=['ros2', 'control', 'load_controller', '--set-state', 'active',
    #          'joint_state_broadcaster'],
    #     output='screen'
    # )

    controllers_loaders = []

    # Create launch description and populate
    declared_arguments = []

    return LaunchDescription(
        declared_arguments + nodes + controllers_loaders
    )
