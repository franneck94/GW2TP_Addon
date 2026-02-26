#include <filesystem>
#include <fstream>

#include "Settings.h"
#include "Shared.h"

const char *SHOW_WINDOW = "ShowWindow";
const char *FORGE_VERSION = "ForgeVersion";
const char *BACKEND_VERSION = "BackendVersion";

namespace Settings
{
    std::mutex Mutex;
    json Settings = json::object();

    void Load(std::filesystem::path SettingsPath)
    {
        if (!std::filesystem::exists(SettingsPath))
        {
            return;
        }

        Settings::Mutex.lock();
        {
            try
            {
                std::ifstream file(SettingsPath);
                Settings = json::parse(file);
                file.close();
            }
            catch (json::parse_error &ex)
            {
                Globals::APIDefs->Log(ELogLevel_WARNING, "GW2TP", "Settings.json could not be parsed.");
                Globals::APIDefs->Log(ELogLevel_WARNING, "GW2TP", ex.what());
            }
        }
        Settings::Mutex.unlock();

        /* Widget */
        if (!Settings[SHOW_WINDOW].is_null())
            Settings[SHOW_WINDOW].get_to<bool>(ShowWindow);
        if (!Settings[FORGE_VERSION].is_null())
            Settings[FORGE_VERSION].get_to<std::string>(ForgeVersion);
        if (!Settings[BACKEND_VERSION].is_null())
            Settings[BACKEND_VERSION].get_to<std::string>(BackendVersion);
    }

    void Save(std::filesystem::path SettingsPath)
    {
        Settings::Mutex.lock();
        {
            Settings[SHOW_WINDOW] = ShowWindow;
            Settings[FORGE_VERSION] = ForgeVersion;
            Settings[BACKEND_VERSION] = BackendVersion;

            std::ofstream file(SettingsPath);
            file << Settings.dump(1, '\t') << std::endl;

            file.close();
        }
        Settings::Mutex.unlock();
    }

    void ToggleShowWindow(std::filesystem::path SettingsPath)
    {
        ShowWindow = !ShowWindow;

        Save(SettingsPath);
    }

    bool ShowWindow = true;
    std::string ForgeVersion = "0.1.0";
    std::string BackendVersion = "0.1.0";
}
