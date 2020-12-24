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

/*
 * Parts of code has been taken from
 *      Edo Jelavic
 *      Institute: ETH Zurich, Robotic Systems Lab
 */

#ifndef BOTANBOT_GRID_MAP__BOTANBOT_GRID_MAP_HPP_
#define BOTANBOT_GRID_MAP__BOTANBOT_GRID_MAP_HPP_

#include <vector>
#include <string>
#include <memory>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "geometry_msgs/msg/point32.hpp"
#include "botanbot_utilities/pcl_helpers.hpp"
#include <grid_map_ros/grid_map_ros.hpp>


/**
 * @brief
 *
 */
namespace botanbot_grid_map
{


/**
 * @brief
 *
 */
class BotanbotGridMap : public rclcpp::Node
{
public:
/**
 * @brief Construct a new Botanbot Grid Map object
 *
 */
  BotanbotGridMap();

/**
 * @brief Destroy the Botanbot Grid Map object
 *
 */
  ~BotanbotGridMap();

/**
 * @brief
 *
 */
  void startWaypointFollowing();

  /**
   * @brief
   *
   * @param linearGridMapIndex
   * @param gridMapData
   */
  void  processGridMapCell(
    grid_map::GridMap * grid_map,
    const unsigned int linearGridMapIndex,
    grid_map::Matrix * gridMapData) const;

  /**
   * @brief Get the Pointcloud Inside Grid Map Cell Border object
   *
   * @param index
   * @return pcl::PointCloud<pcl::PointXYZ>::Ptr
   */
  pcl::PointCloud<pcl::PointXYZ>::Ptr  getPointcloudInsideGridMapCellBorder(
    const grid_map::Index & index) const;


/**
 * @brief Allocates space for the point clouds.  These point clouds are then filled by
 * dispatchWorkingCloudToGridMapCells function.
 *
 */
  void allocateSpaceForCloudsInsideCells(grid_map::GridMap * grid_map);

  /**
   * @brief Makes a matrix of empty point clouds where each cell in the matrix
   * corresponds to a cell in the grid map. The functions iterates over the working
   * point cloud, checks for each point which cell in the grid map it falls
   * within and then adds that point to the correct point cloud in the matrix of point
   * clouds. The decision which cell in the matrix point cloud a point belongs to
   * or not is made by looking at the xy coordinates of the point in the input point cloud
   * and x-y borders of the cell in the grid map.
   *
   */
  void dispatchWorkingCloudToGridMapCells(
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud,
    grid_map::GridMap * grid_map);

  /*!
* Given a point cloud it computes the elevation from it. The algorithm is suited for 2.5 D
* maps. Function finds all the clusters and for each of them it computes a mean of point
* positions. Then it looks at z value of the means and takes the lowest one.
* @param[in] point cloud that is entirely contained inside a grid map cell
* @return elevation value computed from the input point cloud. It will return NaN if no clusters
* have been found or an empty cloud is passed in.
*/
  double calculateElevationFromPointsInsideGridMapCell(
    pcl::PointCloud<pcl::PointXYZ>::Ptr cloud) const;

protected:
  rclcpp::TimerBase::SharedPtr timer_;


  // Matrix of point clouds. Each point cloud has only points that fall within a grid map cell.
  std::vector<std::vector<pcl::PointCloud<pcl::PointXYZ>::Ptr>> pointcloudWithinGridMapCell_;
};
}  // namespace botanbot_grid_map

#endif  // BOTANBOT_GRID_MAP__BOTANBOT_GRID_MAP_HPP_
