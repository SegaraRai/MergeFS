#pragma comment(lib, "LibMergeFS.lib")

#define NOMINMAX

#include "../SDK/LibMergeFS.h"

#include "IdGenerator.hpp"

#include <deque>
#include <functional>
#include <iostream>
#include <optional>
#include <regex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <fcntl.h>
#include <io.h>

using namespace std::literals;

using CONFIG_ID = DWORD;


IdGenerator<CONFIG_ID> gConfigIdGenerator(1);
std::unordered_map<CONFIG_ID, std::deque<std::wstring>> gConfigMap;
std::optional<CONFIG_ID> gCurrentConfigId;


std::deque<std::wstring> SplitCommand(const std::wstring& str) {
  static const std::wregex reToken(L"\"[^\"]+\"|\\S+");
  //static const std::wregex reToken(L"(?:\\S|\\\\.)+|\\\"(?:[^\\\"]|\\\\.)+\\\"");
  //static const std::wregex reReplaceEscapeSequences(L"\\\\(.)");

  std::deque<std::wstring> elements;
  for (std::wsregex_iterator itr(str.cbegin(), str.cend(), reToken), end; itr != end; itr++) {
    const auto rStr = itr->str();
    std::wstring str = rStr.size() > 2 && rStr[0] == L'"' ? rStr.substr(1, rStr.size() - 2) : rStr;
    //str = regex_replace(str, reReplaceEscapeSequences, L"$1");
    elements.push_back(str);
  }

  return elements;
}


int CommandHelp(const std::deque<std::wstring>& args) {
  if (args.empty()) {
    std::wcout << L"add plugin" << std::endl;
    std::wcout << L"remove plugin" << std::endl;
    std::wcout << L"add mount" << std::endl;
    std::wcout << L"remove mount" << std::endl;
    std::wcout << L"add source" << std::endl;
    std::wcout << L"remove source" << std::endl;
    std::wcout << L"mount" << std::endl;
    std::wcout << L"unmount" << std::endl;
    return 0;
  }
  return 0;
}


int CommandSelect(const std::deque<std::wstring>& args) {
  if (args.size() != 2) {
    std::wcout << L"error: invalid arguments"sv << std::endl;
    return 0;
  }

  const std::wstring type = args[0];
  const auto value = stoi(args[1]);

  if (type == L"config") {
    if (!gConfigIdGenerator.has(value)) {
      std::wcout << L"error: invalid config id"sv << std::endl;
      return 0;
    }
    gCurrentConfigId = value;
  } else {
    std::wcout << L"error: invalid type"sv << std::endl;
    return 0;
  }

  return 0;
}


int CommandAdd(const std::deque<std::wstring>& args) {
  if (args.size() != 2) {
    std::wcout << L"error: invalid arguments"sv << std::endl;
    return 0;
  }

  const std::wstring type = args[0];
  const std::wstring value = args[1];

  if (type == L"plugin"sv) {
    PLUGIN_ID pluginId;
    const auto ret = AddSourcePlugin(value.c_str(), false, &pluginId);
    if (!ret) {
      std::wcout << L"error: failed to load source plugin"sv << std::endl;
      return 0;
    }
    std::wcout << L"loaded source plugin: "sv << pluginId << std::endl;
  } else if (type == L"config") {
    const CONFIG_ID configId = gConfigIdGenerator.allocate();
    gConfigMap.emplace(configId, std::deque<std::wstring>{});
    std::wcout << L"created config: "sv << configId << std::endl;
    gCurrentConfigId = configId;
    std::wcout << L"selected config "sv << configId << std::endl;
  } else if (type == L"source") {
    if (!gCurrentConfigId) {
      std::wcout << L"error: select configuration"sv << std::endl;
      return 0;
    }
    const auto configId = gCurrentConfigId.value();
    gConfigMap.at(configId).emplace_back(value);
    std::wcout << L"added "sv << value << L" to config "sv << configId << std::endl;
  }

  return 0;
}


int CommandList(const std::deque<std::wstring>& args) {
  if (args.size() != 1) {
    std::wcout << L"error: invalid arguments"sv << std::endl;
    return 0;
  }

  const std::wstring type = args[0];

  if (type == L"plugin"sv) {

  } else if (type == L"config") {

  } else if (type == L"source") {

  } else if (type == L"mount") {

  }
  return 0;
}


int CommandRemove(const std::deque<std::wstring>& args) {
  if (args.size() != 2) {
    std::wcout << L"error: invalid arguments"sv << std::endl;
    return 0;
  }

  const std::wstring type = args[0];
  const auto value = stoi(args[1]);

  if (type == L"plugin"sv) {

  } else if (type == L"config") {

  } else if (type == L"source") {

  }
  return 0;
}


int CommandReorder(const std::deque<std::wstring>& args) {
  // TODO;
  return 0;
}


int CommandMount(const std::deque<std::wstring>& args) {
  if (args.size() != 2) {
    std::wcout << L"error: invalid arguments"sv << std::endl;
    return 0;
  }
  const auto configId = stoi(args[0]);
  const auto mountPoint = args[1];

  if (!gConfigMap.count(configId)) {
    std::wcout << L"error: no such config"sv << std::endl;
    return 0;
  }

  const auto& config = gConfigMap.at(configId);

  std::vector<MOUNT_SOURCE_INITIALIZE_INFO> mountSources(config.size());
  for (std::size_t i = 0; i < config.size(); i++) {
    mountSources[i] = {
      config.at(i).c_str(),
      {},
      nullptr,
      nullptr,
    };
  }

  MOUNT_INITIALIZE_INFO mountInitializeInfo{
    mountPoint.c_str(),
    TRUE,
    L"metadata",
    TRUE,
    FALSE,
    static_cast<DWORD>(mountSources.size()),
    mountSources.data(),
  };
  MOUNT_ID mountId;
  if (!Mount(&mountInitializeInfo, [](MOUNT_ID mountId, const MOUNT_INFO* mountInfo, int dokanMainResult) noexcept -> void {
    std::wcout << L"info: dokanMainResult = "sv << dokanMainResult << L" at mountId "sv << mountId << std::endl;
  }, &mountId)) {
    std::wcout << L"error: failed to mount"sv << std::endl;
    return 0;
  }
  std::wcout << L"mounted "sv << configId << L" into "sv << mountPoint << L" ("sv << mountId << L")"sv << std::endl;
  return 0;
}


int CommandUnmount(const std::deque<std::wstring>& args) {
  if (args.size() != 1) {
    std::wcout << L"error: invalid arguments"sv << std::endl;
    return 0;
  }

  const auto value = std::stoi(args[0]);

  if (!SafeUnmount(value)) {
    std::wcout << L"error: failed to safe unmount "sv << value << std::endl;
    return 0;
  }

  if (!Unmount(value)) {
    std::wcout << L"error: failed to unmount "sv << value << std::endl;
    return 0;
  }

  std::wcout << L"unmounted "sv << value << std::endl;

  return 0;
}


std::unordered_map<std::wstring, std::function<int(const std::deque<std::wstring>& args)>> gCommandMap{
  {L"help"s, CommandHelp},
  {L"?"s, CommandHelp},
  {L"add"s, CommandAdd},
  {L"list"s, CommandList},
  {L"remove"s, CommandRemove},
  {L"reorder"s, CommandReorder},
  {L"mount"s, CommandMount},
  {L"unmount"s, CommandUnmount},
};


int wmain(int argc, wchar_t* argv[]) {
  Init();

  std::ios_base::sync_with_stdio(false);
  _setmode(_fileno(stdout), _O_U8TEXT);
  _setmode(_fileno(stderr), _O_U8TEXT);
  _setmode(_fileno(stdin), _O_WTEXT);

  for (std::wstring line; (std::wcout << L"MFSCC>"sv), std::getline(std::wcin, line);) {
    auto commands = SplitCommand(line);
    if (commands.empty()) {
      continue;
    }

    const std::wstring command = commands.at(0);
    commands.pop_front();

    if (!gCommandMap.count(command)) {
      std::wcout << L"error: no such command"sv << std::endl;
      continue;
    }

    gCommandMap.at(command)(commands);
  }

  UnmountAll();
  Uninit();
}
