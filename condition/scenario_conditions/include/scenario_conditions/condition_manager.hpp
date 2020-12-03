#ifndef SCENARIO_CONDITION_CONDITION_MANAGER_H_INCLUDED
#define SCENARIO_CONDITION_CONDITION_MANAGER_H_INCLUDED

#include <rclcpp/rclcpp.hpp>
#include <pluginlib/class_loader.hpp>

#include <scenario_conditions/condition_base.hpp>
#include <scenario_conditions/condition_visualizer.hpp>
#include <scenario_intersection/intersection_manager.hpp>
#include <scenario_utility/scenario_utility.hpp>

namespace scenario_conditions
{
  class ConditionManager
  {
  public:
    ConditionManager(YAML::Node node, std::shared_ptr<ScenarioAPI> api_ptr, rclcpp::Node::SharedPtr node_ptr);

    simulation_is update(
      const std::shared_ptr<scenario_intersection::IntersectionManager>&);
    simulation_is update();

    using condition_type
      = std::shared_ptr<scenario_conditions::ConditionBase>;

    void applyVisitorForSuccessConditions(const std::function<void (std::shared_ptr<ConditionBase>)>& visitor);
    void applyVisitorForFailureConditions(const std::function<void (std::shared_ptr<ConditionBase>)>& visitor);

  private:
    condition_type loadPlugin(YAML::Node node, std::shared_ptr<ScenarioAPI> api_ptr);

    std::vector<condition_type> success_conditions_,
                                failure_conditions_;

    ConditionVisualizer visualizer;
  };
}  // namespace scenario_conditions

#endif  // SCENARIO_CONDITION_CONDITION_MANAGER_H_INCLUDED