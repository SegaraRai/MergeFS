#pragma once

#include <string>

#include <yaml-cpp/yaml.h>

#include "EncodingConverter.hpp"


namespace YAML {
  template<>
  struct convert<std::wstring> {
    static Node encode(const std::wstring& rhs) {
      Node node;
      node = WStringtoUTF8(rhs);
      return node;
    }

    static bool decode(const Node& node, std::wstring& rhs) {
      if (!node.IsScalar()) {
        return false;
      }
      rhs = UTF8toWString(node.as<std::string>());
      return true;
    }
  };
}
