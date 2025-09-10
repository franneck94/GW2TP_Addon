#ifndef SETTINGS_H
#define SETTINGS_H

#include <mutex>

#include "nlohmann/json.hpp"
using json = nlohmann::json;

#include "mumble/Mumble.h"
#include "nexus/Nexus.h"
#include "rtapi/RTAPI.hpp"

extern const char *SHOW_WINDOW;

namespace Settings
{
    extern std::mutex Mutex;
    extern json Settings;

    void Load(std::filesystem::path SettingsPath);
    void Save(std::filesystem::path SettingsPath);

    void ToggleShowWindow(std::filesystem::path SettingsPath);

    extern bool ShowWindow;
}

#endif
