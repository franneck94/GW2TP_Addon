#include "Shared.h"

namespace Globals
{
    AddonAPI *APIDefs = nullptr;
    NexusLinkData *NexusLink = nullptr;
    RTAPI::RealTimeData *RTAPIData = nullptr;

    const char *KB_TOGGLE_GW2TP = "KB_TOGGLE_GW2TP";
    const char *ADDON_NAME = "GW2TP";

    std::filesystem::path AddonPath = {};
    std::filesystem::path SettingsPath = {};

    std::vector<uint32_t> CurrentlyPressedKeys;
    PROCESS_INFORMATION ForgeProcessInfo = {};
    bool ForgeProcessActive = false;
}
