// Copyright 2020 Tier IV, Inc.
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

#ifndef SCENARIO_CONDITIONS_CONDITION_BASE_H_INCLUDED
#define SCENARIO_CONDITIONS_CONDITION_BASE_H_INCLUDED

#include <yaml-cpp/yaml.h>

#include "scenario_api/scenario_api_core.hpp"
#include "scenario_intersection/intersection_manager.hpp"
#include "scenario_utility/indentation.hpp"

namespace scenario_conditions
{
class ConditionBase
{
public:
  ConditionBase() = default;

  ConditionBase(const std::string & type, std::size_t occurrence = 0)
    : type_ { type }
    // , name_ { type + "Condition(" + std::to_string(occurrence) + ")" }
  {}

  virtual ~ConditionBase() = default;

  template<typename T>
  using Comparator = std::function<bool (const T &, const T &)>;

  virtual bool update(const std::shared_ptr<scenario_intersection::IntersectionManager> &) = 0;
  virtual bool configure(YAML::Node node, std::shared_ptr<ScenarioAPI> api_ptr) = 0;

  const std::string & getName() const noexcept {return name_;}

  const auto& rename(const std::string& new_name)
  {
    return name_ = new_name;
  }

  const bool getResult() const noexcept { return result_; }

  const std::string & getType() const noexcept {return type_;}

  // friend std::ostream& operator <<(std::ostream& os, const ConditionBase& datum)
  // {
  //   return os << indent
  //             << "{ Name: \x1b[36m"
  //             << std::quoted(datum.name_)
  //             << "\x1b[0m, Value: "
  //             << (datum.result_ ? "\x1b[32m" : "\x1b[31m")
  //             << std::boolalpha << datum.result_
  //             << "\x1b[0m },\n";
  // }

protected:
  std::shared_ptr<ScenarioAPI> api_ptr_;
  YAML::Node node_;

  bool configured_ = false;
  bool keep_ = false;
  bool result_ = false;

  std::string type_;
  std::string name_;
};

}  // namespace scenario_conditions

#endif  // SCENARIO_CONDITIONS_CONDITION_BASE_H_INCLUDED