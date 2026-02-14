#include <cmath>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

#include <DirectXMath.h>
#include <Windows.h>

#include "imgui.h"
#include "mumble/Mumble.h"
#include "nexus/Nexus.h"
#include "rtapi/RTAPI.hpp"

#include "GW2TP_Hover_data.h"
#include "GW2TP_Normal_data.h"
#include "Render.h"
#include "Settings.h"
#include "Shared.h"
#include "Version.h"
#include "resource.h"

namespace dx = DirectX;

void AddonLoad(AddonAPI *aApi);
void AddonUnload();
void AddonRender();
void AddonOptions();

HMODULE hSelf;
AddonDefinition AddonDef{};
Render render{Settings::ShowWindow};

void ToggleShowWindowGW2TP(const char *keybindIdentifier)
{
    Settings::ToggleShowWindow(SettingsPath);
}

void RegisterQuickAccessShortcut()
{
    APIDefs->Log(ELogLevel_DEBUG, ADDON_NAME, "Registering GW2TP quick access shortcut");
    APIDefs->AddShortcut("SHORTCUT_GW2TP", "TEX_GW2TP_NORMAL", "TEX_GW2TP_HOVER", KB_TOGGLE_GW2TP, "Toggle GW2TP Window");
}

void DeregisterQuickAccessShortcut()
{
    APIDefs->Log(ELogLevel_DEBUG, ADDON_NAME, "Deregistering GW2TP quick access shortcut");
    APIDefs->RemoveShortcut("SHORTCUT_GW2TP");
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        hSelf = hModule;
        break;
    case DLL_PROCESS_DETACH:
        break;
    case DLL_THREAD_ATTACH:
        break;
    case DLL_THREAD_DETACH:
        break;
    }
    return TRUE;
}

extern "C" __declspec(dllexport) AddonDefinition *GetAddonDef()
{
    AddonDef.Signature = -1245535;
    AddonDef.APIVersion = NEXUS_API_VERSION;
    AddonDef.Name = "GW2TP";
    AddonDef.Version.Major = MAJOR;
    AddonDef.Version.Minor = MINOR;
    AddonDef.Version.Build = BUILD;
    AddonDef.Version.Revision = REVISION;
    AddonDef.Author = "Franneck.1274";
    AddonDef.Description = "API Fetch from https://gw2tp-production.up.railway.app/";
    AddonDef.Load = AddonLoad;
    AddonDef.Unload = AddonUnload;
    AddonDef.Flags = EAddonFlags_None;
    AddonDef.Provider = EUpdateProvider_GitHub;
    AddonDef.UpdateLink = "https://github.com/franneck94/Gw2TP";

    return &AddonDef;
}

void OnAddonLoaded(int *aSignature)
{
    if (!aSignature)
    {
        return;
    }

    Settings::Load(SettingsPath);
}
void OnAddonUnloaded(int *aSignature)
{
    if (!aSignature)
    {
        return;
    }
}

void AddonLoad(AddonAPI *aApi)
{
    APIDefs = aApi;
    ImGui::SetCurrentContext((ImGuiContext *)APIDefs->ImguiContext);
    ImGui::SetAllocatorFunctions((void *(*)(size_t, void *))APIDefs->ImguiMalloc, (void (*)(void *, void *))APIDefs->ImguiFree); // on imgui 1.80+

    NexusLink = (NexusLinkData *)APIDefs->GetResource("DL_NEXUS_LINK");
    RTAPIData = (RTAPI::RealTimeData *)APIDefs->GetResource(DL_RTAPI);
    APIDefs->RegisterRender(ERenderType_Render, AddonRender);
    APIDefs->RegisterRender(ERenderType_OptionsRender, AddonOptions);
    AddonPath = APIDefs->GetAddonDirectory("GW2TP");
    SettingsPath = APIDefs->GetAddonDirectory("GW2TP/settings.json");
    std::filesystem::create_directory(AddonPath);
    Settings::Load(SettingsPath);
    Settings::ShowWindow = false;

    APIDefs->LoadTextureFromMemory("TEX_GW2TP_NORMAL",
                                   (void *)GW2TP_NORMAL,
                                   GW2TP_NORMAL_size,
                                   nullptr);
    APIDefs->LoadTextureFromMemory("TEX_GW2TP_HOVER",
                                   (void *)GW2TP_HOVER,
                                   GW2TP_HOVER_size,
                                   nullptr);
    APIDefs->RegisterKeybindWithString(KB_TOGGLE_GW2TP, ToggleShowWindowGW2TP, "(null)");
    RegisterQuickAccessShortcut();
}
void AddonUnload()
{
    APIDefs->DeregisterRender(AddonOptions);
    APIDefs->DeregisterRender(AddonRender);

    NexusLink = nullptr;
    RTAPIData = nullptr;

    Settings::Save(SettingsPath);

    DeregisterQuickAccessShortcut();
    APIDefs->DeregisterKeybind(KB_TOGGLE_GW2TP);
}

void AddonRender()
{
    if ((!NexusLink) || (!NexusLink->IsGameplay) || (!Settings::ShowWindow))
    {
        return;
    }

    render.data.requesting();
    render.data.storing();
    render.render();
}

void AddonOptions()
{
}
