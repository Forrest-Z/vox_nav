// Copyright (c) 2018 Intel Corporation
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

#ifndef botanbot_pose_navigator__PLUGINS__ACTION__Follow_pathACTION_HPP_
#define botanbot_pose_navigator__PLUGINS__ACTION__Follow_pathACTION_HPP_

#include <string>

#include "botanbot_msgs/action/follow_path.hpp"
#include "nav_msgs/msg/path.h"
#include "botanbot_pose_navigator/action_client_node.hpp"

namespace botanbot_pose_navigator
{
using FollowPath = botanbot_msgs::action::FollowPath;

class FollowPathNode : public BtActionNode<FollowPath>
{
public:
  FollowPathNode(
    const std::string & xml_tag_name,
    const std::string & action_name,
    const BT::NodeConfiguration & conf)
  : BtActionNode<FollowPath>(xml_tag_name, action_name, conf)
  {
    config().blackboard->set("path_updated", false);
  }

  static BT::PortsList providedPorts()
  {
    return providedBasicPorts(
      {
        BT::InputPort<nav_msgs::msg::Path>("path", "Path to follow"),
        BT::InputPort<std::string>("controller_id", ""),
      });
  }

  void on_tick()
  {
    getInput("path", goal_.path);
    getInput("controller_id", goal_.controller_id);
  }

  void on_wait_for_result()
  {
    // Check if the goal has been updated
    if (config().blackboard->get<bool>("path_updated")) {
      // Reset the flag in the blackboard
      config().blackboard->set("path_updated", false);

      // Grab the new goal and set the flag so that we send the new goal to
      // the action server on the next loop iteration
      getInput("path", goal_.path);
      goal_updated_ = true;
    }
  }
};

} // namespace botanbot_pose_navigator

#endif  // botanbot_pose_navigator__PLUGINS__ACTION__Follow_pathACTION_HPP_
