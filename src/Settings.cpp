#include <filesystem>
#include <fstream>

#include "Settings.h"
#include "Shared.h"

const char *SHOW_WINDOW = "ShowWindow";

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
                APIDefs->Log(ELogLevel_WARNING, "GW2TP", "Settings.json could not be parsed.");
                APIDefs->Log(ELogLevel_WARNING, "GW2TP", ex.what());
            }
        }
        Settings::Mutex.unlock();

        /* Widget */
        if (!Settings[SHOW_WINDOW].is_null())
        {
            Settings[SHOW_WINDOW].get_to<bool>(ShowWindow);
        }
    }

    void Save(std::filesystem::path SettingsPath)
    {
        Settings::Mutex.lock();
        {
            std::ofstream file(SettingsPath);
            file << Settings.dump(1, '\t') << std::endl;
            file.close();
        }
        Settings::Mutex.unlock();
    }

    void ToggleShowWindow(std::filesystem::path SettingsPath)
    {
        ShowWindow = !ShowWindow;
        Settings[SHOW_WINDOW] = ShowWindow;
        Save(SettingsPath);
    }

    bool ShowWindow = true;
}
