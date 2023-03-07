#pragma once

#include <memory>
#include <string>
#include <map>
#include <algorithm>
#include <math.h>
#include <string>

#include "rclcpp/rclcpp.hpp"
#include "rclcpp/publisher.hpp"
#include "rclcpp/subscription.hpp"
#include "rclcpp_action/rclcpp_action.hpp"
#include "rclcpp/qos.hpp"
#include "rclcpp_lifecycle/lifecycle_publisher.hpp"
#include "Eigen/Dense"
#include "Eigen/Geometry"

#include "std_msgs/msg/string.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "geometry_msgs/msg/point.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "geometry_msgs/msg/point_stamped.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/quaternion_stamped.hpp"
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "sensor_msgs/msg/nav_sat_fix.hpp"

#include "plansys2_executor/ActionExecutorClient.hpp"

#include "anafi_uav_interfaces/msg/move_by_command.hpp"
#include "anafi_uav_interfaces/msg/move_to_command.hpp"
#include "anafi_uav_interfaces/msg/ekf_output.hpp"
#include "anafi_uav_interfaces/srv/get_search_positions.hpp"
#include "anafi_uav_interfaces/action/move_to_ned.hpp"

using namespace std::chrono_literals;
using LifecycleNodeInterface = rclcpp_lifecycle::node_interfaces::LifecycleNodeInterface;


class SearchActionNode : public plansys2::ActionExecutorClient
{
public:
  SearchActionNode() 
  : plansys2::ActionExecutorClient("search_action_node", 250ms)
  , node_activated_(false)
  , action_running_(false)
  , search_point_idx_(0)
  {
    std::string location_prefix = "locations.";
    this->declare_parameter(location_prefix + "names", std::vector<std::string>());
    std::vector<std::string> locations_names = this->get_parameter(location_prefix + "names").as_string_array();
    
    std::string pos_ne_prefix = location_prefix + "pos_ne.";
    for(std::string loc_name : locations_names)
    {
      this->declare_parameter(pos_ne_prefix + loc_name, std::vector<double>());      
    }
    this->declare_parameter(location_prefix + "location_radius_m"); // Fail if not found in config
    radius_of_acceptance_ = this->get_parameter(location_prefix + "location_radius_m").as_double();

    // Services
    search_positions_client_ = this->create_client<anafi_uav_interfaces::srv::GetSearchPositions>("/generate_search_waypoints");  

    // Actions
    move_action_client_ = rclcpp_action::create_client<anafi_uav_interfaces::action::MoveToNED>(shared_from_this(), "/move_to_ned");
  }

  // Lifecycle-events
  LifecycleNodeInterface::CallbackReturn on_activate(const rclcpp_lifecycle::State &);
  LifecycleNodeInterface::CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);


private:
  // State 
  bool node_activated_;
  bool action_running_;

  int search_point_idx_;
  double radius_of_acceptance_;

  std::string search_location_;
  geometry_msgs::msg::Point search_center_point_;
  std::vector<geometry_msgs::msg::Point> search_points_;
  std::map<std::string, geometry_msgs::msg::Point> ned_locations_;

  // Services
  rclcpp::Client<anafi_uav_interfaces::srv::GetSearchPositions>::SharedPtr search_positions_client_;
  // Service to inform the mission controller that a location is searched or not

  // Actions
  using MoveGoalHandle = rclcpp_action::ClientGoalHandle<anafi_uav_interfaces::action::MoveToNED>;
  using MoveFeedback = const std::shared_ptr<const anafi_uav_interfaces::action::MoveToNED::Feedback>;

  rclcpp_action::Client<anafi_uav_interfaces::action::MoveToNED>::SharedPtr move_action_client_;
  std::shared_future<MoveGoalHandle::SharedPtr> future_move_goal_handle_;
  MoveGoalHandle::SharedPtr move_goal_handle_;
  anafi_uav_interfaces::action::MoveToNED::Goal move_goal_;


  // Private functions
  /**
   * @brief Initializes the location to move to, by loading from a config file and into 
   * the map @p ned_locations_
   */
  void init_locations_();


  /**
   * @brief Overload of function in ActionExecutorClient. Nothing handled within this function 
   * for now, as most of the actions implemented through on_activate()
   */
  void do_work();


  /**
   * @brief Checks whether the preconditions are satisfied for searching an area 
   */
  bool check_search_preconditions_();


  /**
   * @brief Gets a set of coordinates (NED) centered around the @p search_center_point_  
   */
  bool get_search_positions_();

}; // SearchActionNode
