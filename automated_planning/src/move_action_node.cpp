#include "automated_planning/move_action_node.hpp"

LifecycleNodeInterface::CallbackReturn
MoveActionNode::on_activate(const rclcpp_lifecycle::State & previous_state)
{
  // Get the goal
  const std::string goal_location = get_arguments()[2]; 
  std::map<std::string, geometry_msgs::msg::PointStamped>::iterator it_goal_pos = locations_.find(goal_location);
  if(it_goal_pos == locations_.end())
  {
    finish(false, 0.0, "Unable to find goal location!");
    RCLCPP_WARN(this->get_logger(), "Goal location not found!");
    return LifecycleNodeInterface::CallbackReturn::FAILURE;
  }
  goal_position_ned_ = std::get<1>(*it_goal_pos);
  start_distance_ = get_position_error_ned().norm();

  send_feedback(0.0, "Starting move-action!");

  // Activating lifecyclepublishers
  cmd_move_by_pub_->on_activate();
  cmd_move_to_pub_->on_activate();

  // Stupid variable to get things to work
  node_activated_ = true;
  
  return ActionExecutorClient::on_activate(previous_state);
}


LifecycleNodeInterface::CallbackReturn
MoveActionNode::on_deactivate(const rclcpp_lifecycle::State &)
{
  RCLCPP_INFO(this->get_logger(), "Deactivating");

  // Deactivate publishers
  cmd_move_by_pub_->on_deactivate();
  cmd_move_to_pub_->on_deactivate();

  node_activated_ = false;

  return LifecycleNodeInterface::CallbackReturn::SUCCESS;
}



void MoveActionNode::do_work()
{
  // This is where the fun begins...
  // Good luck!

  if(! node_activated_)
  {
    return;
  }

  // Checking the preconditions to prevent race-conditions during activation 
  static bool preconditions_success = false;
  static int num_preconditions_failed = 0;
  const int max_preconditions_failed = 5;

  if(! preconditions_success)
  {
    // Checking inside of the if-loop to prevent checking the preconditions
    // for every iteration after it has first passed
    preconditions_success = check_move_preconditions_(); 
    num_preconditions_failed++;
    if(num_preconditions_failed >= max_preconditions_failed)
    {
      num_preconditions_failed = 0;
      preconditions_success = false;

      RCLCPP_ERROR(this->get_logger(), "Preconditions for move failed!");
      finish(false, 0.0, "Preconditions for move failed!");
    }
    return;
  }
  num_preconditions_failed = 0;

  // The drone will transition using hovering, to ensure that the move-commands are 
  // valid. Problems were encountered if the move-commands were assigned when the 
  // drone was flying, as move commands were entered with respect to a non-zero 
  // roll / pitch
  // Difficult to get this in a readable format
  switch (move_state_)
  {
    case MoveState::HOVER:
    {
      if(! check_hovering_())
      {
        RCLCPP_INFO(this->get_logger(), "Starting hovering");
        hover_();
        return;
      }

      if(! check_goal_achieved_())
      {
        // Target not achieved
        RCLCPP_WARN(this->get_logger(), "Hovering while not achieved goal position...");
        move_state_ = MoveState::MOVE;
        return;
      }

      // Target achieved
      RCLCPP_INFO(this->get_logger(), "Hovering close to goal position");
      finish(true, 1.0, "Position reached");

      break;
    }

    case MoveState::MOVE:
    {
      bool hovering = check_hovering_();
      bool goal_achieved = check_goal_achieved_();

      if(goal_achieved)
      {
        RCLCPP_INFO(this->get_logger(), "Goal achieved during move");
        move_state_ = MoveState::HOVER;
      }
      else if(hovering)
      {
        Eigen::Vector3d pos_error_ned = get_position_error_ned();
        Eigen::Vector3d pos_error_body = attitude_.toRotationMatrix().transpose() * pos_error_ned; 

        double distance = pos_error_ned.norm(); 
        const double max_distance = 100;

        if(distance > max_distance)
        {
          RCLCPP_ERROR(this->get_logger(), "Norm of body position error (%f) exceeds maximum expected norm (%f)", distance, max_distance);
          finish(false, -1.0, "Position error norm exceeds maximum");

          return;
        }

        // Using a counter to ensure that commands are not sent frequenctly
        // and to ensure that the drone must be hovering for some time before 
        // a new move-command is transmitted
        static int start_move_counter = 0;
        const int max_start_move_counter = 10;
        
        if(start_move_counter >= max_start_move_counter)
        {
          float dx = -static_cast<float>(pos_error_body.x());
          float dy = -static_cast<float>(pos_error_body.y());
          float dz = -static_cast<float>(pos_error_body.z());

          RCLCPP_WARN(this->get_logger(), "Move ordered: x = %f, y = %f, z = %f", dx, dy, dz);

          pub_moveby_cmd(dx, dy, dz);
          start_move_counter = 0;

          send_feedback(1.0 - distance / start_distance_, "Movement ordered...");
        }
        else 
        {
          start_move_counter++;
        }
      }

      break;
    }

    default:
    {
      RCLCPP_ERROR(this->get_logger(), "Invalid value occured");
      finish(false, 0.0, "Invalid value occured");
      hover_();
      break;
    }
  }
}


bool MoveActionNode::check_move_preconditions_()
{
  // Currently assume that it can always move if the drone is either flying or hovering
  return (anafi_state_.compare("FS_HOVERING") == 0) || (anafi_state_.compare("FS_FLYING") == 0);
}


bool MoveActionNode::check_movement_along_vector_(const Eigen::Vector3d& move_vec)
{
  // The following conditions are checked: 
  // - the drone is flying
  // - the velocity vector is comparable to the positional error vector 

  // To ensure comparable results, the norm of the positional error and the velocity
  // vector must be sufficiently large

  // Check that the drone is flying
  if(anafi_state_.compare("FS_FLYING") != 0)
  {
    RCLCPP_ERROR(this->get_logger(), "Anafi not in flying state");
    return false;
  }

  // Check that the movement-vector is sufficiently large
  double min_vec_norm = 0.5;
  if(move_vec.norm() < min_vec_norm)
  {
    RCLCPP_WARN(this->get_logger(), "Movement vector too small");
    return false;
  }

  // Check nonzero velocity
  Eigen::Vector3d vel_vec{ polled_vel_.twist.linear.x, polled_vel_.twist.linear.y, polled_vel_.twist.linear.z };
  double min_vel = 0.1; // Below this, it is more tracking / accurate control
  if(vel_vec.norm() < min_vel)
  {
    RCLCPP_WARN(this->get_logger(), "Velocity too low");
    return false;
  }

  // Check the horizontal angle to be small enough
  Eigen::Vector2d hor_move_vec{ move_vec.x(), move_vec.y() };
  Eigen::Vector2d hor_vel_vec{ vel_vec.x(), vel_vec.y() };
  double norm_hor_move_vec = hor_move_vec.norm();
  double norm_hor_vel_vec = hor_vel_vec.norm();

  double min_hor_norm = 0.01;
  if(norm_hor_move_vec < min_hor_norm || norm_hor_vel_vec < min_hor_norm)
  {
    RCLCPP_WARN(this->get_logger(), "Horizontal norm too low");
    return false;
  }

  const double pi = 3.14159265358979323846;
  double max_angle = 10 * pi / 180.0;

  // Not the most efficient method of using acos. atan would be better
  double angle = std::acos((hor_move_vec.dot(hor_vel_vec) / (norm_hor_move_vec * norm_hor_vel_vec)));
  return std::abs(angle) <= max_angle;
}


void MoveActionNode::hover_()
{
  if(check_hovering_())
  {
    // Already hovering
    return;
  }
  pub_moveby_cmd(0.0, 0.0, 0.0);
}


bool MoveActionNode::check_hovering_()
{
  return anafi_state_.compare("FS_HOVERING") == 0; 
}


bool MoveActionNode::check_goal_achieved_()
{
  Eigen::Vector3d pos_error_ned = get_position_error_ned();
  double distance = pos_error_ned.norm();
  return distance <= radius_of_acceptance_;
}


Eigen::Vector3d MoveActionNode::get_position_error_ned()
{
  geometry_msgs::msg::Point pos_ned = position_ned_.point;
  geometry_msgs::msg::Point goal_pos_ned = goal_position_ned_.point;

  double x_diff = pos_ned.x - goal_pos_ned.x;
  double y_diff = pos_ned.y - goal_pos_ned.y;
  double z_diff = pos_ned.z - goal_pos_ned.z;

  Eigen::Vector3d error_ned;
  error_ned << x_diff, y_diff, z_diff;
  return error_ned;
}


void MoveActionNode::pub_moveby_cmd(float dx, float dy, float dz)
{
  anafi_uav_interfaces::msg::MoveByCommand moveby_cmd = anafi_uav_interfaces::msg::MoveByCommand();
  moveby_cmd.header.stamp = this->now();
  moveby_cmd.dx = dx;
  moveby_cmd.dy = dy;
  moveby_cmd.dz = dz;
  moveby_cmd.dyaw = 0;

  cmd_move_by_pub_->publish(moveby_cmd);
}


void MoveActionNode::pub_moveto_cmd(double lat, double lon, double h)
{
  anafi_uav_interfaces::msg::MoveToCommand moveto_cmd = anafi_uav_interfaces::msg::MoveToCommand();
  moveto_cmd.header.stamp = this->now();
  moveto_cmd.latitude = lat;
  moveto_cmd.longitude = lon;
  moveto_cmd.altitude = h;
  moveto_cmd.heading = 0;
  moveto_cmd.orientation_mode = 0; // Drone will not orient itself during movement
  
  cmd_move_to_pub_->publish(moveto_cmd);
}


void MoveActionNode::anafi_state_cb_(std_msgs::msg::String::ConstSharedPtr state_msg)
{
  std::string state = state_msg->data;
  auto it = std::find_if(possible_anafi_states_.begin(), possible_anafi_states_.end(), [state](std::string str){ return state.compare(str) == 0; });
  if(it == possible_anafi_states_.end())
  {
    // No state found
    return;
  }
  anafi_state_ = state;
}


void MoveActionNode::ekf_cb_(anafi_uav_interfaces::msg::EkfOutput::ConstSharedPtr ekf_msg)
{
  // Assume that the message is more recent for now... (bad assumption)
  ekf_output_.header.stamp = ekf_msg->header.stamp;
  ekf_output_.x_r = ekf_msg->x_r;
  ekf_output_.y_r = ekf_msg->y_r;
  ekf_output_.z_r = ekf_msg->z_r;
  ekf_output_.u_r = ekf_msg->u_r;
  ekf_output_.v_r = ekf_msg->v_r;
  ekf_output_.w_r = ekf_msg->w_r;
}


void MoveActionNode::ned_pos_cb_(geometry_msgs::msg::PointStamped::ConstSharedPtr ned_pos_msg)
{
  // Assume that the message is more recent for now... (bad assumption)
  position_ned_.header.stamp = ned_pos_msg->header.stamp;
  position_ned_.point = ned_pos_msg->point;
}


void MoveActionNode::gnss_data_cb_(sensor_msgs::msg::NavSatFix::ConstSharedPtr gnss_data_msg)
{
  (void) gnss_data_msg;
}


void MoveActionNode::attitude_cb_(geometry_msgs::msg::QuaternionStamped::ConstSharedPtr attitude_msg)
{
  Eigen::Vector3d v(attitude_msg->quaternion.x, attitude_msg->quaternion.y, attitude_msg->quaternion.z);
  attitude_.w() = attitude_msg->quaternion.w;
  attitude_.vec() = v;
  attitude_.normalize();
}


void MoveActionNode::polled_vel_cb_(geometry_msgs::msg::TwistStamped::ConstSharedPtr vel_msg)
{
  // Assume that the message is more recent for now... (bad assumption)
  polled_vel_.header.stamp = vel_msg->header.stamp;
  polled_vel_.twist = vel_msg->twist;
}



void MoveActionNode::init_locations_()
{
  // TODO:
  // - add more locations
  // - add the locations into a config file or similar

  geometry_msgs::msg::PointStamped loc_ned_pos;
  loc_ned_pos.header.frame_id = "/map";
  loc_ned_pos.header.stamp = this->now();

  // Have some safety with respect to the altitude 
  loc_ned_pos.point.z = -5.0; 

  // Home location - this can be changed afterwards by getting some data from the 
  // Revolt as it moves
  loc_ned_pos.point.x = 0.0;
  loc_ned_pos.point.y = 0.0;
  locations_["h1"] = loc_ned_pos;

  loc_ned_pos.point.x = 20.0;
  loc_ned_pos.point.y = 0.0;
  locations_["a1"] = loc_ned_pos;

  loc_ned_pos.point.x = 0.0;
  loc_ned_pos.point.y = 20.0;
  locations_["a2"] = loc_ned_pos;
}



int main(int argc, char ** argv)
{
  rclcpp::init(argc, argv);
  auto node = std::make_shared<MoveActionNode>();

  node->set_parameter(rclcpp::Parameter("action_name", "move"));
  node->trigger_transition(lifecycle_msgs::msg::Transition::TRANSITION_CONFIGURE);

  try
  { 
    rclcpp::spin(node->get_node_base_interface());
  }
  catch(...) {}
  
  rclcpp::shutdown();

  return 0;
}
