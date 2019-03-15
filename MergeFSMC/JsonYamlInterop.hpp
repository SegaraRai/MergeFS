#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>
#include <yaml-cpp/yaml.h>

using namespace std::literals;


namespace nlohmann {
  template<>
  struct adl_serializer<YAML::Node> {
    static void to_json(json& j, const YAML::Node& y) {
      const auto& tag = y.Tag();

      if (!tag.empty() && tag[0] == '!') {
        // bool
        if (tag == "!<tag:yaml.org,2002:bool>"sv || tag == "!!bool"sv) {
          j = y.as<bool>();
          return;
        }

        // float
        if (tag == "!<tag:yaml.org,2002:float>"sv || tag == "!!float"sv) {
          j = y.as<double>();
          return;
        }

        // int
        if (tag == "!<tag:yaml.org,2002:int>"sv || tag == "!!int"sv) {
          j = y.as<std::int64_t>();
          return;
        }

        // null
        if (tag == "!<tag:yaml.org,2002:null>"sv || tag == "!!null"sv) {
          j = nullptr;
          return;
        }

        // str
        if (tag == "!<tag:yaml.org,2002:str>"sv || tag == "!!str"sv || tag == "!"sv) {
          j = y.as<std::string>();
          return;
        }
      }

      // auto detect
      switch (y.Type()) {
        case YAML::NodeType::Undefined:
          // TODO?
          throw std::runtime_error("cannot convert YAML::Node to json: NodeType is Undefined");

        case YAML::NodeType::Null:
          j = nullptr;
          break;

        case YAML::NodeType::Scalar:
          // try int
          try {
            j = y.as<std::int64_t>();
            break;
          } catch (YAML::BadConversion&) {}

          // try double
          try {
            j = y.as<double>();
            break;
          } catch (YAML::BadConversion&) {}

          // try bool
          try {
            j = y.as<bool>();
            break;
          } catch (YAML::BadConversion&) {}

          // try string
          j = y.as<std::string>();
          
          break;

        case YAML::NodeType::Sequence:
          j = json::array();
          for (const auto& ey : y) {
            json ej;
            to_json(ej, ey);
            j.emplace_back(ej);
          }
          break;

        case YAML::NodeType::Map:
          j = json::object();
          for (const auto& ey : y) {
            const auto k = ey.first.as<std::string>();
            const auto& vy = ey.second;
            json vj;
            to_json(vj, vy);
            j.emplace(k, vj);
          }
          break;
      }
    }


    static void from_json(const json& j, YAML::Node& y) {
      switch (j.type()) {
        case json::value_t::null:
          y.reset();
          break;

        case json::value_t::object:
          y.reset();
          for (auto& [k, vj] : j.items()) {
            YAML::Node vy;
            from_json(vj, vy);
            y[k] = vy;
          }
          break;

        case json::value_t::array:
          y.reset();
          for (auto& ej : j) {
            YAML::Node ey;
            from_json(ej, ey);
            y.push_back(ey);
          }
          break;

        case json::value_t::string:
          y = j.get<std::string>();
          break;

        case json::value_t::boolean:
          y = j.get<bool>();
          break;

        case json::value_t::number_integer:
          y = j.get<std::int64_t>();
          break;

        case json::value_t::number_unsigned:
          y = j.get<std::uint64_t>();
          break;

        case json::value_t::number_float:
          y = j.get<double>();
          break;
      }
    }
  };
}
