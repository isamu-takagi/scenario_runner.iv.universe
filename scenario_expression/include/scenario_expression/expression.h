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

#ifndef INCLUDED_SCENARIO_EXPRESSION_EXPRESSION_H
#define INCLUDED_SCENARIO_EXPRESSION_EXPRESSION_H

#include "pluginlib/class_loader.hpp"
#include "scenario_api/scenario_api_core.hpp"
#include "scenario_conditions/condition_base.hpp"
#include "scenario_entities/entity_manager.hpp"
#include "scenario_intersection/intersection_manager.hpp"
#include "scenario_utility/indentation.hpp"
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <boost/lexical_cast.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/smart_ptr/shared_ptr.hpp>
#include <functional>
#include <ios>
#include <iostream>
#include <sstream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>

// Forward declarations
class ScenarioAPI;

namespace scenario_expression
{

class Context
{
#define boilerplate(TYPE, NAME) \
private: \
  std::shared_ptr<TYPE> NAME ## _; \
 \
public: \
  void define(const std::shared_ptr<TYPE>&NAME) \
  { \
    NAME ## _ = NAME; \
  } \
 \
  const auto & NAME ## _pointer() const noexcept \
  { \
    return NAME ## _; \
  } \
 \
  auto & NAME() \
  { \
    if (NAME ## _) \
    { \
      return *NAME ## _; \
    } \
    else \
    { \
      SCENARIO_ERROR_THROW( \
        CATEGORY(), \
        "No " #NAME " defined, but scenario execution requires this."); \
    } \
  } static_assert(true, "")

  boilerplate(ScenarioAPI, api);
  boilerplate(scenario_entities::EntityManager, entities);
  boilerplate(scenario_intersection::IntersectionManager, intersections);

#undef boilerplate
};

/* -----------------------------------------------------------------------------
 *
 * EXPRESSION
 *   <Expression> = <Literal>
 *                | <Logical>
 *                | <Procedure Call>
 *                | <Sequential>
 *                | <Parallel>
 *
 * LITERAL EXPRESSION
 *   <Literal> = <Boolean> | <Number>
 *
 *   <Number> = <Double Float>
 *
 * LOGICAL EXPRESSION
 *   <Logical> = <N-Ary Logical Operator> [ <Test>* ]
 *             | <Unary Logical Operator> { <Test> }
 *
 *   <N-Ary Logical Operator> = <All> | <Any>
 *   <Unary Logical Operator> = <Not>
 *
 *   <Test> = <Expression>
 *
 * PROCEDURE CALL
 *   <Procedure Call> = <Action Call> | <Predicate Call>
 *
 * SEQUENTIAL EXPRESSION
 *   <Sequential>
 *
 * PARALLEL EXPRESSION
 *   <Parallel>
 *
 * The value of the test is Boolean, which returns whether the return value of
 * the expression is equal to false or not. Note that the return value of the
 * expression is not necessarily Boolean.
 *
 * -------------------------------------------------------------------------- */

template<typename T>
class Literal;

using Boolean = Literal<bool>;

class All;
class Any;

class Predicate;

class Expression
{
  friend Boolean;

  friend Predicate;

  friend All;
  friend Any;

public:
  Expression()
  : data{nullptr},
    count{0}
  {}

  Expression(const Expression & expression)
  : data{expression.data},
    count{0}
  {
    if (data) {
      ++(data->count);
    }
  }

  virtual ~Expression()
  {
    if (data && --(data->count) == 0) {
      delete data;
    }
  }

  virtual const std::string& type() const
  {
    static const std::string result { "Expression" };
    return result;
  }

  Expression & operator=(const Expression & rhs)
  {
    Expression e {rhs};
    swap(e);
    return *this;
  }

  virtual Expression evaluate(Context & context)
  {
    return data ? data->evaluate(context) : *this;
  }

  template<typename T, typename ... Ts>
  static auto make(Ts && ... xs)
  {
    Expression e {};
    e.remake(new T {std::forward<decltype(xs)>(xs)...});
    return e;
  }

  void swap(Expression & e) noexcept
  {
    std::swap(data, e.data);
  }

  virtual boost::property_tree::ptree property(
    const std::string& prefix = "", std::size_t occurrence = 0) const
  {
    if (data)
    {
      return (*data).property(prefix, occurrence);
    }
    else
    {
      boost::property_tree::ptree result {};
      result.push_back(
        std::make_pair("", boost::property_tree::ptree()));
      return result;
    }
  }

  friend std::ostream & operator<<(std::ostream & os, const Expression & expression)
  {
    return boost::property_tree::write_json(os, expression.data->property()), os;
  }

  virtual operator bool() const noexcept
  {
    return data ? static_cast<bool>(*data) : false;
  }

protected:
  Expression(std::integral_constant<decltype(0), 0>)
  : data{nullptr},
    count{1}
  {}

private:
  void remake(Expression * e)
  {
    if (data && --(data->count) == 0) {
      delete data;
    }

    data = e;
  }

  Expression * data;

  std::size_t count;
};

Expression read(Context &, const YAML::Node &);

template<typename T>
class Literal
  : public Expression
{
  friend Expression;

  T value;

protected:
  Literal(const T & value)
  : Expression{std::integral_constant<decltype(0), 0>()},
    value{value}
  {}

  Literal(const Literal & rhs)
  : Expression{std::integral_constant<decltype(0), 0>()},
    value{rhs.value}
  {}

  virtual ~Literal() = default;

  const std::string& type() const override
  {
    static const std::string result { "Literal" };
    return result;
  }

  Expression evaluate(Context&) override
  {
    return *this;
  }

  operator bool() const noexcept override
  {
    return value;
  }
};

#define DEFINE_N_ARY_LOGICAL_EXPRESSION(NAME, OPERATOR, BASE_CASE)             \
class NAME                                                                     \
  : public Expression                                                          \
{                                                                              \
  friend Expression;                                                           \
                                                                               \
  OPERATOR<bool> combine;                                                      \
                                                                               \
  std::vector<Expression> operands;                                            \
                                                                               \
public:                                                                        \
  const std::string& type() const override                                     \
  {                                                                            \
    static const std::string result { #NAME };                                 \
    return result;                                                             \
  }                                                                            \
                                                                               \
protected:                                                                     \
  NAME(const NAME& rhs)                                                        \
    : Expression { std::integral_constant<decltype(0), 0>() }                  \
    , operands { rhs.operands }                                                \
  {}                                                                           \
                                                                               \
  NAME(Context& context, const YAML::Node& node)                               \
    : Expression { std::integral_constant<decltype(0), 0>() }                  \
  {                                                                            \
    if (node.IsSequence())                                                     \
    {                                                                          \
      for (const auto& each : node)                                            \
      {                                                                        \
        operands.push_back(read(context, each));                               \
      }                                                                        \
    }                                                                          \
  }                                                                            \
                                                                               \
  virtual ~NAME() = default;                                                   \
                                                                               \
  Expression evaluate(Context& context) override                               \
  {                                                                            \
    return                                                                     \
      Expression::make<Boolean>(                                               \
      std::accumulate(                                                         \
        operands.begin(), operands.end(), BASE_CASE,                           \
        [&](const auto& lhs, auto&& rhs)                                       \
        {                                                                      \
          return combine(lhs, rhs.evaluate(context));                          \
        }));                                                                   \
  }                                                                            \
                                                                               \
  boost::property_tree::ptree property(                                        \
    const std::string& prefix, std::size_t occurrence) const override          \
  {                                                                            \
    std::unordered_map<std::string, std::size_t> occurrences {};               \
                                                                               \
    boost::property_tree::ptree result {};                                     \
                                                                               \
    if (not operands.empty())                                                  \
    {                                                                          \
      for (const auto& each : operands)                                        \
      {                                                                        \
        const auto property {                                                  \
          each.property(                                                       \
            prefix + type() + "(" + std::to_string(occurrence) + ")/",         \
            occurrences[each.data->type()]++)                                  \
        };                                                                     \
                                                                               \
        try                                                                    \
        {                                                                      \
          property.get_child("Name");                                          \
          result.push_back(std::make_pair("", property));                      \
        }                                                                      \
        catch (...)                                                            \
        {                                                                      \
          for (const auto& each : property.get_child(""))                      \
          {                                                                    \
            result.push_back(                                                  \
              std::make_pair("", each.second));                                \
          }                                                                    \
        }                                                                      \
      }                                                                        \
    }                                                                          \
    else                                                                       \
    {                                                                          \
      result.push_back(                                                        \
        std::make_pair("", boost::property_tree::ptree()));                    \
    }                                                                          \
                                                                               \
    return result;                                                             \
  }                                                                            \
}

DEFINE_N_ARY_LOGICAL_EXPRESSION(All, std::logical_and, true);
DEFINE_N_ARY_LOGICAL_EXPRESSION(Any, std::logical_or, false);

template<typename PluginBase>
class Procedure
  : public Expression
{
  friend Expression;

protected:
  std::shared_ptr<PluginBase> plugin;

  Procedure()
  : Expression{std::integral_constant<decltype(0), 0>()}
  {}

  Procedure(const Procedure & proc)
  : Expression{std::integral_constant<decltype(0), 0>()},
    plugin{proc.plugin}
  {}

  virtual ~Procedure() = default;

  const std::string& type() const override
  {
    return (*plugin).getType();
  }

  boost::property_tree::ptree property(
    const std::string& prefix, std::size_t occurrence) const override
  {
    if ((*plugin).getName().empty())
    {
      (*plugin).rename(prefix + (*plugin).getType() + "(" + std::to_string(occurrence) + ")");
    }

    return (*plugin).property();
  }

  virtual pluginlib::ClassLoader<PluginBase> & loader() const = 0;

  auto load(const std::string & name)
  {
    for (const auto & declaration : loader().getDeclaredClasses()) {
      if (loader().getName(declaration) == name) {
        return loader().createSharedInstance(declaration);
      }
    }

    SCENARIO_ERROR_THROW(CATEGORY(), "Failed to load Procedure " << name);
  }

  Expression evaluate(Context & context) override
  {
    return Expression::make<Boolean>(plugin->update(context.intersections_pointer()));
  }
};

class Predicate
  : public Procedure<scenario_conditions::ConditionBase>
{
  friend Expression;

protected:
  using Procedure::Procedure;

  Predicate(Context & context, const YAML::Node & node)
  try
  : Procedure{}
  {
    if (plugin = load(read_essential<std::string>(node, "Type") + "Condition")) {
      plugin->configure(node, context.api_pointer());
    } else {
      SCENARIO_ERROR_THROW(
        CATEGORY(),
        "Failed to load procedure of type " << read_essential<std::string>(node, "Type"));
    }
  } catch (...) {
    SCENARIO_ERROR_RETHROW(CATEGORY(), "Syntax error: malformed predicate.\n\n" << node << "\n");
  }

  virtual ~Predicate() = default;

  pluginlib::ClassLoader<scenario_conditions::ConditionBase> & loader() const
  {
    static pluginlib::ClassLoader<scenario_conditions::ConditionBase> loader {
      "scenario_conditions", "scenario_conditions::ConditionBase"
    };
    return loader;
  }
};

} // namespace scenario_expression

#endif // INCLUDED_SCENARIO_EXPRESSION_EXPRESSION_H
