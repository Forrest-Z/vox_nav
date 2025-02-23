// Copyright (c) 2021 Norwegian University of Life Sciences, Fetullah Atas
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

#include <vox_nav_map_server/cost_regression_utils.hpp>

#include <utility>
#include <vector>

namespace vox_nav_map_server
{

pcl::PointCloud<pcl::PointXYZRGB>::Ptr denoise_segmented_cloud(
  const pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud,
  const double radius,
  const double tolerated_divergence_rate,
  const int min_num_neighbours)
{
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr denoised_cloud(new pcl::PointCloud<pcl::PointXYZRGB>);
  pcl::KdTreeFLANN<pcl::PointXYZRGB> kdtree;
  kdtree.setInputCloud(cloud);

  for (size_t i = 0; i < cloud->points.size(); i++) {
    pcl::PointXYZRGB searchPoint = cloud->points[i];
    std::vector<int> pointIdxRadiusSearch;
    std::vector<float> pointRadiusSquaredDistance;
    if (kdtree.radiusSearch(
        searchPoint, radius, pointIdxRadiusSearch,
        pointRadiusSquaredDistance) > min_num_neighbours)
    {
      pcl::PointCloud<pcl::PointXYZRGB>::Ptr neighbours(new pcl::PointCloud<pcl::PointXYZRGB>);
      for (std::size_t j = 0; j < pointIdxRadiusSearch.size(); ++j) {
        neighbours->points.push_back(cloud->points[pointIdxRadiusSearch[j]]);
      }
      int num_traversable_neighbours = 0;
      for (std::size_t j = 0; j < neighbours->points.size(); ++j) {
        if (neighbours->points[j].g) {
          num_traversable_neighbours++;
        }
      }
      double proportion_of_traversable_neighbours =
        static_cast<double>(num_traversable_neighbours) /
        static_cast<double>(neighbours->points.size());
      if (proportion_of_traversable_neighbours >
        (1.0 - tolerated_divergence_rate) &&
        searchPoint.r)
      {
        searchPoint.r = 0;
        searchPoint.g = 255;
      } else if (proportion_of_traversable_neighbours <
        tolerated_divergence_rate && searchPoint.g)
      {
        searchPoint.r = 255;
        searchPoint.g = 0;
      }
    }
    denoised_cloud->points.push_back(searchPoint);
  }
  denoised_cloud->height = 1;
  denoised_cloud->width = denoised_cloud->points.size();
  return denoised_cloud;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr get_non_traversable_points(
  const pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud)
{
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr pure_non_traversable_points(
    new pcl::PointCloud<pcl::PointXYZRGB>);
  for (size_t i = 0; i < cloud->points.size(); i++) {
    if (cloud->points[i].r) {
      pure_non_traversable_points->points.push_back(cloud->points[i]);
    }
  }
  pure_non_traversable_points->height = 1;
  pure_non_traversable_points->width = pure_non_traversable_points->points.size();
  return pure_non_traversable_points;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr get_traversable_points(
  const pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud)
{
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr pure_traversable_points(
    new pcl::PointCloud<pcl::PointXYZRGB>);
  for (size_t i = 0; i < cloud->points.size(); i++) {
    if (cloud->points[i].g) {
      pure_traversable_points->points.push_back(cloud->points[i]);
    }
  }
  pure_traversable_points->height = 1;
  pure_traversable_points->width = pure_traversable_points->points.size();
  return pure_traversable_points;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr uniformly_sample_cloud(
  const pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud,
  const double radius)
{
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr uniformly_sampled_cloud(
    new pcl::PointCloud<pcl::PointXYZRGB>);
  pcl::UniformSampling<pcl::PointXYZRGB> filter;
  filter.setInputCloud(cloud);
  filter.setRadiusSearch(radius);
  filter.filter(*uniformly_sampled_cloud);
  for (auto && i : uniformly_sampled_cloud->points) {
    i.b = 255;
    i.g = 0;
    i.r = 0;
  }
  uniformly_sampled_cloud->height = 1;
  uniformly_sampled_cloud->width = uniformly_sampled_cloud->points.size();
  return uniformly_sampled_cloud;
}

std::vector<std::pair<pcl::PointXYZRGB,
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr>> decompose_traversability_cloud(
  const pcl::PointCloud<pcl::PointXYZRGB>::Ptr pure_traversable_pcl,
  const pcl::PointCloud<pcl::PointXYZRGB>::Ptr uniformly_sampled_nodes,
  double radius)
{
  // Neighbors within radius search
  std::vector<int> pointIdxRadiusSearch;
  std::vector<float> pointRadiusSquaredDistance;
  pcl::KdTreeFLANN<pcl::PointXYZRGB> kdtree;
  kdtree.setInputCloud(pure_traversable_pcl);

  std::vector<std::pair<pcl::PointXYZRGB,
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr>> decomposed_cells;

  for (auto && searchPoint : uniformly_sampled_nodes->points) {
    pcl::PointCloud<pcl::PointXYZRGB>::Ptr points_within_this_cell(
      new pcl::PointCloud<pcl::PointXYZRGB>);
    if (kdtree.radiusSearch(
        searchPoint, radius, pointIdxRadiusSearch,
        pointRadiusSquaredDistance) > 0)
    {
      for (std::size_t i = 0; i < pointIdxRadiusSearch.size(); ++i) {
        auto crr_point = pure_traversable_pcl->points[pointIdxRadiusSearch[i]];
        crr_point.r = pointIdxRadiusSearch.size() * 3;
        crr_point.g = 0;
        crr_point.b = 0;
        points_within_this_cell->points.push_back(crr_point);
      }
    }
    points_within_this_cell->height = 1;
    points_within_this_cell->width = points_within_this_cell->points.size();

    decomposed_cells.push_back(std::make_pair(searchPoint, points_within_this_cell));
  }
  return decomposed_cells;
}

pcl::ModelCoefficients fit_plane_to_cloud(
  const pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud,
  const double dist_thes)
{
  pcl::ModelCoefficients::Ptr coefficients(new pcl::ModelCoefficients);
  if (cloud->points.size() > 2) {
    pcl::PointIndices::Ptr inliers(new pcl::PointIndices);
    // Create the segmentation object
    pcl::SACSegmentation<pcl::PointXYZRGB> seg;
    // Optional
    seg.setOptimizeCoefficients(true);
    // Mandatory
    seg.setModelType(pcl::SACMODEL_PLANE);
    seg.setMethodType(pcl::SAC_RANSAC);
    seg.setDistanceThreshold(dist_thes);
    seg.setInputCloud(cloud);
    seg.segment(*inliers, *coefficients);
    if (inliers->indices.size() == 0) {
      PCL_ERROR("Could not estimate a planar model for the given dataset.");
    }
  } else {
    coefficients->values.push_back(0.0);
    coefficients->values.push_back(0.0);
    coefficients->values.push_back(0.0);
    coefficients->values.push_back(0.0);
  }
  return *coefficients;
}

pcl::PointCloud<pcl::PointXYZRGB>::Ptr set_cloud_color(
  const pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud,
  const std::vector<double> colors)
{
  pcl::PointCloud<pcl::PointXYZRGB>::Ptr new_colored_cloud(
    new pcl::PointCloud<pcl::PointXYZRGB>);
  // Set color of points to given colors
  for (auto && i : cloud->points) {
    i.r = colors[0];
    i.g = colors[1];
    i.b = colors[2];
    new_colored_cloud->points.push_back(i);
  }
  new_colored_cloud->height = 1;
  new_colored_cloud->width = new_colored_cloud->points.size();
  return new_colored_cloud;
}

std::vector<double> absolute_rpy_from_plane(
  const pcl::ModelCoefficients plane_model)
{
  std::vector<double> absolute_rpy({0.0, 0.0, 0.0});
  const double kRAD2DEG = 180.0 / M_PI;

  auto vector_magnitude =
    std::sqrt(
    std::pow(plane_model.values[0], 2) +
    std::pow(plane_model.values[1], 2) +
    std::pow(plane_model.values[2], 2));
  auto roll = std::abs(plane_model.values[0] / vector_magnitude * kRAD2DEG);
  auto pitch = std::abs(plane_model.values[1] / vector_magnitude * kRAD2DEG);
  auto yaw = std::abs(plane_model.values[2] / vector_magnitude * kRAD2DEG);

  absolute_rpy[0] = roll;
  absolute_rpy[1] = pitch;
  absolute_rpy[2] = yaw;
  return absolute_rpy;
}

double average_point_deviation_from_plane(
  const pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud,
  const pcl::ModelCoefficients plane_model)
{
  double total_dist = 0.0;
  for (auto && i : cloud->points) {
    double curr_point_dist_to_plane = std::abs(
      plane_model.values[0] * i.x +
      plane_model.values[1] * i.y +
      plane_model.values[2] * i.z + plane_model.values[3]) /
      std::sqrt(
      std::pow(plane_model.values[0], 2) +
      std::pow(plane_model.values[1], 2) +
      std::pow(plane_model.values[2], 2));
    total_dist += curr_point_dist_to_plane;
  }
  double average_point_deviation_from_plane = total_dist /
    static_cast<double>(cloud->points.size());
  return average_point_deviation_from_plane;
}

double max_energy_gap_in_cloud(
  const pcl::PointCloud<pcl::PointXYZRGB>::Ptr cloud,
  const double m,
  const double v)
{
  pcl::PointXYZRGB lower_bound_point(cloud->points.front());
  pcl::PointXYZRGB upper_bound_point(cloud->points.front());
  for (size_t i = 0; i < cloud->points.size(); i++) {
    if (cloud->points[i].z < lower_bound_point.z) {
      lower_bound_point = cloud->points[i];
    }
    if (cloud->points[i].z > upper_bound_point.z) {
      upper_bound_point = cloud->points[i];
    }
  }
  double max_energy_gap = m * 9.82 * std::abs(upper_bound_point.z - lower_bound_point.z) +
    0.5 * m * std::pow(v, 2);
  return max_energy_gap;
}
}  // namespace vox_nav_map_server
