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
#include "KeyboardCapture.h"
#include "Render.h"
#include "Settings.h"
#include "Shared.h"
#include "Version.h"

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
    Settings::ToggleShowWindow(Globals::SettingsPath);
}

void RegisterQuickAccessShortcut()
{
    Globals::APIDefs->Log(ELogLevel_DEBUG, Globals::ADDON_NAME, "Registering GW2TP quick access shortcut");
    Globals::APIDefs->AddShortcut("SHORTCUT_GW2TP", "TEX_GW2TP_NORMAL", "TEX_GW2TP_HOVER", Globals::KB_TOGGLE_GW2TP, "Toggle GW2TP Window");
}

void DeregisterQuickAccessShortcut()
{
    Globals::APIDefs->Log(ELogLevel_DEBUG, Globals::ADDON_NAME, "Deregistering GW2TP quick access shortcut");
    Globals::APIDefs->RemoveShortcut("SHORTCUT_GW2TP");
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
        return;

    Settings::Load(Globals::SettingsPath);
}
void OnAddonUnloaded(int *aSignature)
{
    if (!aSignature)
        return;
}

void AddonLoad(AddonAPI *aApi)
{
    Globals::APIDefs = aApi;
    ImGui::SetCurrentContext((ImGuiContext *)Globals::APIDefs->ImguiContext);
    ImGui::SetAllocatorFunctions((void *(*)(size_t, void *))Globals::APIDefs->ImguiMalloc, (void (*)(void *, void *))Globals::APIDefs->ImguiFree); // on imgui 1.80+

    Globals::NexusLink = (NexusLinkData *)Globals::APIDefs->GetResource("DL_NEXUS_LINK");
    Globals::RTAPIData = (RTAPI::RealTimeData *)Globals::APIDefs->GetResource(DL_RTAPI);
    Globals::APIDefs->RegisterRender(ERenderType_Render, AddonRender);
    Globals::APIDefs->RegisterRender(ERenderType_OptionsRender, AddonOptions);
    Globals::AddonPath = Globals::APIDefs->GetAddonDirectory("GW2TP");
    Globals::SettingsPath = Globals::APIDefs->GetAddonDirectory("GW2TP/settings.json");
    std::filesystem::create_directory(Globals::AddonPath);
    Settings::Load(Globals::SettingsPath);
    Settings::ShowWindow = false;

    // Initialize KeyboardCapture
    KeyboardCapture::GetInstance().Initialize(
        Globals::APIDefs->RegisterWndProc,
        Globals::APIDefs->DeregisterWndProc);

    Globals::APIDefs->LoadTextureFromMemory("TEX_GW2TP_NORMAL",
                                            (void *)GW2TP_NORMAL,
                                            GW2TP_NORMAL_size,
                                            nullptr);
    Globals::APIDefs->LoadTextureFromMemory("TEX_GW2TP_HOVER",
                                            (void *)GW2TP_HOVER,
                                            GW2TP_HOVER_size,
                                            nullptr);
    Globals::APIDefs->RegisterKeybindWithString(Globals::KB_TOGGLE_GW2TP, ToggleShowWindowGW2TP, "(null)");
    RegisterQuickAccessShortcut();
}

void AddonUnload()
{
    Globals::APIDefs->DeregisterRender(AddonOptions);
    Globals::APIDefs->DeregisterRender(AddonRender);

    Globals::NexusLink = nullptr;
    Globals::RTAPIData = nullptr;

    KeyboardCapture::GetInstance().Shutdown();

    if (Globals::ForgeProcessActive)
    {
        TerminateProcess(Globals::ForgeProcessInfo.hProcess, 0);
        CloseHandle(Globals::ForgeProcessInfo.hProcess);
        CloseHandle(Globals::ForgeProcessInfo.hThread);
        Globals::ForgeProcessActive = false;
    }

    Settings::Save(Globals::SettingsPath);

    DeregisterQuickAccessShortcut();
    Globals::APIDefs->DeregisterKeybind(Globals::KB_TOGGLE_GW2TP);
}

void AddonRender()
{
    if ((!Globals::NexusLink) || (!Globals::NexusLink->IsGameplay) || (!Settings::ShowWindow))
        return;

    auto &keyboard = KeyboardCapture::GetInstance();

    if (keyboard.WasKeyPressed(VK_ESCAPE))
    {
        Settings::ShowWindow = false;
        Settings::Save(Globals::SettingsPath);
        return;
    }

    if (keyboard.IsKeyDown(VK_CONTROL) && keyboard.WasKeyPressed('C'))
    {
        if (Globals::ForgeProcessActive)
        {
            Globals::APIDefs->Log(ELogLevel_INFO, Globals::ADDON_NAME, "Interrupting forge python script...");
            CloseHandle(Globals::ForgeProcessInfo.hThread);
            Globals::ForgeProcessActive = false;
        }
    }

    render.data.requesting();
    render.data.storing();
    render.render();
}

void AddonOptions()
{
}
