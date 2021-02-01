// Copyright (c) 2020 Fetullah Atas, Norwegian University of Life Sciences
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

#include <nav_msgs/msg/path.hpp>
#include <botanbot_control/mpc_controller/mpc_controller_wrapper.hpp>


using namespace std;

namespace botanbot_control
{
namespace mpc_controller
{

MPCWrapper::MPCWrapper(rclcpp::Node::SharedPtr parent)
{
  node_ = parent;

  cmd_vel_publisher_ =
    node_->create_publisher<geometry_msgs::msg::Twist>("cmd_vel", 10);

  // Initialize pubs & subs
  plan_publisher_ = node_->create_publisher<visualization_msgs::msg::MarkerArray>("plan", 1);

  interpolated_ref_traj_publisher_ = node_->create_publisher<visualization_msgs::msg::MarkerArray>(
    "interpolated_plan", 1);


  // setup TF buffer and listerner to read transforms
  tf_buffer_ = std::make_shared<tf2_ros::Buffer>(node_->get_clock());
  tf_listener_ = std::make_shared<tf2_ros::TransformListener>(*tf_buffer_);

  reference_traj_ = createTestTraj();
  previous_time_ = node_->now();

  solve();
}

MPCWrapper::~MPCWrapper()
{
}

nav_msgs::msg::Path MPCWrapper::createTestTraj()
{
  nav_msgs::msg::Path test_traj;
  for (int i = 1; i < 21; i++) {
    geometry_msgs::msg::PoseStamped test_pose;
    test_pose.header.frame_id = "map";
    test_pose.header.stamp = node_->now();
    test_pose.pose.position.x = i;
    test_pose.pose.position.y = -i;
    tf2::Quaternion q;
    q.setRPY(0.0, 0.0, std::atan(test_pose.pose.position.y / test_pose.pose.position.x));
    test_pose.pose.orientation = tf2::toMsg(q);
    test_traj.poses.push_back(test_pose);
  }
  return test_traj;
}

void MPCWrapper::configure(
  const rclcpp_lifecycle::LifecycleNode::WeakPtr &,
  std::string name, const std::shared_ptr<tf2_ros::Buffer> &
  /*const std::shared_ptr<nav2_costmap_2d::Costmap2DROS> &*/)
{
}

void MPCWrapper::solve()
{
  auto regulate_max_speed = [this](double kMAX_SPEED) {
      if (twist_.linear.x > kMAX_SPEED) {
        twist_.linear.x = kMAX_SPEED;
      } else if (twist_.linear.x < -kMAX_SPEED) {
        twist_.linear.x = -kMAX_SPEED;
      }
    };
  rclcpp::Duration transfrom_tolerance(std::chrono::seconds(1));

  rclcpp::WallRate rate(10);
  double dt = 1.0 / 10.0;
  double kTARGET_SPEED = 1.0;
  double kMAX_SPEED = 1.0;
  double kL_F = 1.32;    // distance from rear to front axle(m)

  while (rclcpp::ok()) {

    geometry_msgs::msg::PoseStamped curr_robot_pose;
    if (!botanbot_utilities::getCurrentPose(
        curr_robot_pose, *tf_buffer_, "map", "base_link", 0.1))
    {
      RCLCPP_DEBUG(node_->get_logger(), "Current robot pose is not available.");
    }

    auto ref_states = intrpolateTraj(reference_traj_, curr_robot_pose);

    tf2::Quaternion q;
    tf2::fromMsg(curr_robot_pose.pose.orientation, q);
    tf2::Matrix3x3 m(q);
    double r, p, psi;
    m.getRPY(r, p, psi);

    mpc_controller_.updateInitialCondition(
      curr_robot_pose.pose.position.x,
      curr_robot_pose.pose.position.y, psi, 1.0);
    mpc_controller_.updateReference(ref_states[0], ref_states[1], ref_states[2], ref_states[3]);
    mpc_controller_.updatePreviousInput(previous_control_.first, previous_control_.second);
    SolutionResult res = mpc_controller_.solve();

    //  The control output is acceleration but we need to publish speed
    twist_.linear.x += res.control_input.first * (dt);
    //  The control output is steeering angle but we need to publish angular velocity
    //twist_.angular.z = twist_.linear.x / (std::tan(res.control_input.second) / kL_F);
    twist_.angular.z = (twist_.linear.x * res.control_input.second) / kL_F;

    //twist_.linear.x = res.control_input.first;
    //twist_.angular.z = res.control_input.second;

    regulate_max_speed(kMAX_SPEED);
    cmd_vel_publisher_->publish(twist_);
    publihTestTraj();
    publihInterpolatedRefTraj(ref_states);
    previous_control_ = res.control_input;
    rate.sleep();
  }
}

geometry_msgs::msg::TwistStamped MPCWrapper::computeVelocityCommands(
  const geometry_msgs::msg::PoseStamped & pose,
  const geometry_msgs::msg::Twist & velocity)
{
  return geometry_msgs::msg::TwistStamped();
}

void MPCWrapper::setPlan(const nav_msgs::msg::Path & path)
{

}

int MPCWrapper::calculate_nearest_state_index(
  nav_msgs::msg::Path reference_traj, geometry_msgs::msg::PoseStamped curr_robot_pose)
{
  int closest_state_index = -1;
  int closest_state_distance = 10000.0;
  for (int i = 0; i < reference_traj.poses.size(); i++) {
    double curr_distance =
      std::sqrt(
      std::pow(reference_traj.poses[i].pose.position.x - curr_robot_pose.pose.position.x, 2) +
      std::pow(reference_traj.poses[i].pose.position.y - curr_robot_pose.pose.position.y, 2));
    if (curr_distance < closest_state_distance) {
      closest_state_distance = curr_distance;
      closest_state_index = i;
    }
  }
  return closest_state_index;
}

std::vector<std::vector<double>> MPCWrapper::intrpolateTraj(
  const nav_msgs::msg::Path ref_traj,
  geometry_msgs::msg::PoseStamped curr_robot_pose)
{
  int nearsest_taj_state_index =
    calculate_nearest_state_index(ref_traj, curr_robot_pose);

  std::vector<double> interpolated_x_ref, interpolated_y_ref, interpolated_psi_ref;

  int kTRAJHORIZON = 10;
  double kTRAJDT = 0.3;
  double kTARGETSPEED = 1.0;
  for (int i = 0; i < kTRAJHORIZON; i++) {
    interpolated_x_ref.push_back(
      ref_traj.poses[nearsest_taj_state_index].pose.position.x + kTRAJDT * i);
    interpolated_y_ref.push_back(
      ref_traj.poses[nearsest_taj_state_index].pose.position.y - kTRAJDT * i);

    tf2::Quaternion curr_waypoint_psi_quat;
    tf2::fromMsg(ref_traj.poses[i].pose.orientation, curr_waypoint_psi_quat);
    double none, psi;
    tf2::Matrix3x3 curr_waypoint_rot(curr_waypoint_psi_quat);
    curr_waypoint_rot.getRPY(none, none, psi);
    interpolated_psi_ref.push_back(psi);
  }

  std::vector<std::vector<double>> interplated_references;
  std::vector<double> v_ref(interpolated_x_ref.size(), kTARGETSPEED);

  interplated_references.push_back(interpolated_x_ref);
  interplated_references.push_back(interpolated_y_ref);
  interplated_references.push_back(interpolated_psi_ref);
  interplated_references.push_back(v_ref);

  return interplated_references;
}

void
MPCWrapper::publihTestTraj()
{
  visualization_msgs::msg::MarkerArray marker_array;
  auto path_idx = 0;

  for (auto && i : reference_traj_.poses) {
    visualization_msgs::msg::Marker marker;
    marker.header.frame_id = "map";
    marker.header.stamp = rclcpp::Clock().now();
    marker.ns = "path";
    marker.id = path_idx;
    marker.type = visualization_msgs::msg::Marker::CUBE;
    marker.action = visualization_msgs::msg::Marker::ADD;
    marker.lifetime = rclcpp::Duration::from_seconds(0);
    marker.pose = i.pose;
    marker.scale.x = 0.5;
    marker.scale.y = 0.2;
    marker.scale.z = 0.2;
    marker.color.a = 0.6;
    marker.color.r = 0.0;
    marker.color.g = 1.0;
    marker.color.b = 0.0;
    marker_array.markers.push_back(marker);
    path_idx++;
  }
  plan_publisher_->publish(marker_array);
}

void
MPCWrapper::publihInterpolatedRefTraj(std::vector<std::vector<double>> interpolated_ref_traj)
{
  visualization_msgs::msg::MarkerArray marker_array;
  // iterae through Ref raj Xs
  for (int i = 0; i < interpolated_ref_traj[0].size(); i++) {
    visualization_msgs::msg::Marker marker;
    marker.header.frame_id = "map";
    marker.header.stamp = rclcpp::Clock().now();
    marker.ns = "path";
    marker.id = i;
    marker.type = visualization_msgs::msg::Marker::CUBE;
    marker.action = visualization_msgs::msg::Marker::ADD;
    marker.lifetime = rclcpp::Duration::from_seconds(0);
    marker.pose.position.x = interpolated_ref_traj[0][i];
    marker.pose.position.y = interpolated_ref_traj[1][i];
    marker.pose.position.z = 0.1;
    tf2::Quaternion curr_ref_traj_sample_quat;
    curr_ref_traj_sample_quat.setRPY(0, 0, interpolated_ref_traj[2][i]);
    marker.pose.orientation = tf2::toMsg(curr_ref_traj_sample_quat);
    marker.scale.x = 0.2;
    marker.scale.y = 0.1;
    marker.scale.z = 0.1;
    marker.color.a = 0.8;
    marker.color.r = 1.0;
    marker.color.g = 0.0;
    marker.color.b = 0.0;
    marker_array.markers.push_back(marker);
  }
  interpolated_ref_traj_publisher_->publish(marker_array);
}


}  // namespace mpc_controller
}  // namespace botanbot_control


int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<rclcpp::Node>("mpc");
  botanbot_control::mpc_controller::MPCWrapper mpc(node);
  rclcpp::spin(node->get_node_base_interface());

  rclcpp::shutdown();
  return 0;
}
