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
#include "std_msgs/msg/float64.hpp"
#include "geometry_msgs/msg/pose.hpp"
#include "geometry_msgs/msg/point.hpp"
#include "geometry_msgs/msg/pose_with_covariance_stamped.hpp"
#include "geometry_msgs/msg/twist_stamped.hpp"
#include "geometry_msgs/msg/point_stamped.hpp"
#include "geometry_msgs/msg/pose_stamped.hpp"
#include "geometry_msgs/msg/quaternion_stamped.hpp"
#include "nav2_msgs/action/navigate_to_pose.hpp"
#include "sensor_msgs/msg/nav_sat_fix.hpp"
#include "std_srvs/srv/set_bool.hpp"

#include "plansys2_executor/ActionExecutorClient.hpp"

#include "anafi_uav_interfaces/msg/detected_person.hpp"
#include "anafi_uav_interfaces/msg/move_by_command.hpp"
#include "anafi_uav_interfaces/msg/move_to_command.hpp"
#include "anafi_uav_interfaces/msg/ekf_output.hpp"
#include "anafi_uav_interfaces/msg/float32_stamped.hpp"
#include "anafi_uav_interfaces/srv/set_finished_action.hpp"
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
    // Parameters
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

    // this->declare_parameter("search.move_method"); // Fail if not found
    // std::string method = this->get_parameter("search.move_method").as_string();
    // if(method.compare("GNC") != 0)// && method.compare("ANAFI") != 0)
    // {
    //   throw std::domain_error("Invalid method used");
    // }
    // use_gnc_ = method.compare("GNC") == 0; // If false, ANAFI is used

    // Publishers
    goal_position_pub_ = this->create_publisher<geometry_msgs::msg::PointStamped>(
      "/guidance/desired_ned_position", rclcpp::QoS(1).reliable());

    // Subscribers
    using namespace std::placeholders;
    detected_person_sub_ = this->create_subscription<anafi_uav_interfaces::msg::DetectedPerson>(
      "estimate/detected_person", rclcpp::QoS(1).best_effort(), std::bind(&SearchActionNode::detected_person_cb_, this, _1));
    apriltags_detected_sub_ = this->create_subscription<anafi_uav_interfaces::msg::Float32Stamped>(
      "/estimate/aprilTags/num_tags_detected", rclcpp::QoS(1).best_effort(), std::bind(&SearchActionNode::apriltags_detected_cb_, this, _1));  

    // Services
    search_positions_client_ = this->create_client<anafi_uav_interfaces::srv::GetSearchPositions>("/waypoint_generator/generate_search_waypoints");  
    finished_action_client_ = this->create_client<anafi_uav_interfaces::srv::SetFinishedAction>("/mission_controller/set_finished_action");
    enable_velocity_control_client_ = this->create_client<std_srvs::srv::SetBool>("/velocity_controller/service/enable_controller"); 

    // Actions
    move_action_client_ = rclcpp_action::create_client<anafi_uav_interfaces::action::MoveToNED>(shared_from_this(), "/move_to_ned");
  }

  // Lifecycle-events
  LifecycleNodeInterface::CallbackReturn on_activate(const rclcpp_lifecycle::State &);
  LifecycleNodeInterface::CallbackReturn on_deactivate(const rclcpp_lifecycle::State &);


private:
  // State 
  bool node_activated_;

  bool use_gnc_;
  bool action_running_;

  int search_point_idx_;
  double radius_of_acceptance_;

  std::string search_location_;
  geometry_msgs::msg::Point search_center_point_;

  geometry_msgs::msg::Point position_ned_;
  geometry_msgs::msg::Point goal_position_ned_;

  std::vector<geometry_msgs::msg::Point> search_points_;
  std::map<std::string, geometry_msgs::msg::Point> ned_locations_;

  // Storing location and time of last detection
  std::map<std::string, std::tuple<rclcpp::Time, std::string>> detections_;  

  // Publishers
  rclcpp::Publisher<geometry_msgs::msg::PointStamped>::SharedPtr goal_position_pub_;

  // Subscribers
  rclcpp::Subscription<anafi_uav_interfaces::msg::DetectedPerson>::ConstSharedPtr detected_person_sub_;
  rclcpp::Subscription<anafi_uav_interfaces::msg::Float32Stamped>::ConstSharedPtr apriltags_detected_sub_;

  // Services
  rclcpp::Client<anafi_uav_interfaces::srv::GetSearchPositions>::SharedPtr search_positions_client_;
  rclcpp::Client<anafi_uav_interfaces::srv::SetFinishedAction>::SharedPtr finished_action_client_;
  rclcpp::Client<std_srvs::srv::SetBool>::SharedPtr enable_velocity_control_client_;

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
   * @brief Check whether a detection has occured over the last second.
   * Note that it will not distinguish between detecting helipad or person at sea
   */
  bool check_recent_detection();


  /**
   * @brief Gets a set of coordinates (NED) centered around the @p search_center_point_  
   */
  bool get_search_positions_();


  /**
   * @brief Uses the service function for informing the mission controller that an action is 
   * marked as completed by the search action node 
   */
  bool set_search_action_finished_(const std::string& argument="");


  /**
   * @brief Functions copied into this node from the track action server. These are used to 
   * ensure that the drone can use the guidance and velocity control
   * 
   * For whomever comes after: 
   * Using the TrackActionServer does not work. Library error I haven't quite figured out to solve.
   * Originally developed to use an action server, but quickly changed to utilize the GNC instead.
   * Sorry for the mess! 
   */
  bool set_velocity_controller_state_(bool controller_state, const std::string& error_str="");
  bool check_goal_achieved_();
  void hover_();
  void pub_desired_ned_position_(const geometry_msgs::msg::Point& target_position);
  Eigen::Vector3d get_position_error_ned_();
  void ned_pos_cb_(geometry_msgs::msg::PointStamped::ConstSharedPtr ned_pos_msg);


  // Callbacks
  void detected_person_cb_(anafi_uav_interfaces::msg::DetectedPerson::ConstSharedPtr detected_person_msg);
  void apriltags_detected_cb_(anafi_uav_interfaces::msg::Float32Stamped::ConstSharedPtr detection_msg);

}; // SearchActionNode
