#include <windows.h>

#include <shellapi.h>
#include <shlwapi.h>
#include <urlmon.h>
#include <wininet.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shlwapi.lib")

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <ranges>
#include <string>
#include <thread>
#include <vector>

#include "imgui.h"

#include "API.h"
#include "Constants.h"
#include "Data.h"
#include "Render.h"
#include "RenderUI.h"
#include "Settings.h"
#include "Shared.h"

namespace
{
    static const std::string backend_url =
        "https://github.com/franneck94/Gw2TP/releases/download/3.0.0/GW2TP_Python.exe";
    static const std::string latest_backend_version = "3.0.0";

    static const std::string forge_url =
        "https://github.com/franneck94/GW2MysticForge/releases/download/0.2.0/mystic_forge.exe";
    static const std::string latest_forge_version = "0.2.0";

    static const std::string clicker_url =
        "https://github.com/franneck94/GW2_AutoClicker/releases/download/0.1.0/GW2_AutoClicker.exe";
    static const std::string latest_clicker_version = "0.1.0";

    bool VersionIsLower(const std::string current_version, const std::string &latest_version)
    {
        return current_version < latest_version;
    }

    bool DownloadFile(const std::string &url, const std::filesystem::path &outputPath)
    {
        try
        {
            (void)Globals::APIDefs->Log(ELogLevel_DEBUG, "GW2TP", "Started Downloading");

            const auto hr = URLDownloadToFileA(nullptr, url.c_str(), outputPath.string().c_str(), 0, nullptr);
            return SUCCEEDED(hr);
        }
        catch (...)
        {
            (void)Globals::APIDefs->Log(ELogLevel_CRITICAL, "GW2TP", "Downloading failed.");
            return false;
        }
    }

    bool ExtractZipFile(const std::filesystem::path &zipPath, const std::filesystem::path &extractPath)
    {
        try
        {
            (void)Globals::APIDefs->Log(ELogLevel_DEBUG, "GW2TP", "Started Extracting");

            const auto psCommand = "powershell.exe -Command \"Expand-Archive -Path '" + zipPath.string() +
                                   "' -DestinationPath '" + extractPath.string() + "' -Force\"";

            STARTUPINFOA si = {sizeof(si)};
            PROCESS_INFORMATION pi = {};
            si.dwFlags = STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_HIDE;

            if (CreateProcessA(nullptr,
                               const_cast<char *>(psCommand.c_str()),
                               nullptr,
                               nullptr,
                               FALSE,
                               0,
                               nullptr,
                               nullptr,
                               &si,
                               &pi))
            {
                (void)Globals::APIDefs->Log(ELogLevel_INFO, "GW2TP", "Started extraction process.");
                WaitForSingleObject(pi.hProcess, 30000); // Wait max 30 seconds
                DWORD exitCode;
                GetExitCodeProcess(pi.hProcess, &exitCode);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                if (exitCode != 0)
                {
                    auto errorMsg = "Extraction failed with exit code: " + std::to_string(exitCode);
                    (void)Globals::APIDefs->Log(ELogLevel_DEBUG, "GW2TP", errorMsg.c_str());
                }

                return exitCode == 0;
            }

            DWORD createProcessError = GetLastError();
            auto errorMsg = "Create Process Failed with error code: " + std::to_string(createProcessError);
            (void)Globals::APIDefs->Log(ELogLevel_CRITICAL, "GW2TP", errorMsg.c_str());
            return false;
        }
        catch (...)
        {
            (void)Globals::APIDefs->Log(ELogLevel_CRITICAL, "GW2TP", "Extraction failed.");
            return false;
        }
    }

    void DownloadAndExtractDataAsync(const std::filesystem::path &addonPath, const std::string &data_url, const std::string filename)
    {
        std::thread([addonPath, data_url, filename]()
                    {
        try
        {
            auto extract_path = addonPath / filename;

            (void)Globals::APIDefs->Log(ELogLevel_DEBUG, "GW2TP", "Started Download Thread.");

            if (DownloadFile(data_url, extract_path))
                Settings::Save(Globals::SettingsPath);
        }
        catch (...)
        {
            (void)Globals::APIDefs->Log(ELogLevel_DEBUG, "GW2TP", "DownloadAndExtractDataAsync failed.");
        } })
            .detach();
    }

    void DropFiles(const std::filesystem::path &path)
    {
        try
        {
            if (std::filesystem::exists(path) && std::filesystem::is_directory(path))
            {
                for (const auto &entry : std::filesystem::directory_iterator(path))
                {
                    if (entry.is_regular_file())
                    {
                        std::filesystem::remove(entry.path());
                    }
                }

                (void)Globals::APIDefs->Log(ELogLevel_INFO,
                                            "GW2TP",
                                            ("Removed old build files from " + path.string() + " directory").c_str());
            }
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            auto errorMsg = "Error removing old builds from " + path.string() + " directory: " + std::string(e.what());
            (void)Globals::APIDefs->Log(ELogLevel_WARNING, "GW2TP", errorMsg.c_str());
        }
        catch (...)
        {
            (void)Globals::APIDefs->Log(
                ELogLevel_WARNING,
                "GW2TP",
                ("Unknown error while removing old builds from " + path.string() + " directory").c_str());
        }
    }

    void center_next_element(const char *label, bool is_text = false)
    {
        const float windowWidth = ImGui::GetWindowContentRegionWidth();
        const float elementWidth = is_text ? ImGui::CalcTextSize(label).x : ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        ImGui::SetCursorPosX((windowWidth - elementWidth) * 0.5f);
    }

    void center_group_with_width(const float total_width)
    {
        ImGui::SetCursorPosX((ImGui::GetWindowContentRegionWidth() - total_width) * 0.5f);
    }
}

void RenderUI::start_executable(const std::string &exe_path, const std::string &args, bool show_cmd_window)
{
    try
    {
        std::string command = "cmd /k \"" + exe_path + "\"";

        if (!args.empty())
        {
            command += " " + args;
        }

        STARTUPINFOA si = {sizeof(si)};
        PROCESS_INFORMATION pi = {};
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = show_cmd_window ? SW_SHOWNORMAL : SW_HIDE;

        if (CreateProcessA(nullptr,
                           const_cast<char *>(command.c_str()),
                           nullptr,
                           nullptr,
                           FALSE,
                           0,
                           nullptr,
                           nullptr,
                           &si,
                           &pi))
        {
            (void)Globals::APIDefs->Log(ELogLevel_INFO, "GW2TP", ("Started executable: " + exe_path).c_str());
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);
        }
        else
        {
            DWORD createProcessError = GetLastError();
            auto errorMsg = "Failed to start executable with error code: " + std::to_string(createProcessError);
            (void)Globals::APIDefs->Log(ELogLevel_CRITICAL, "GW2TP", errorMsg.c_str());
        }
    }
    catch (...)
    {
        (void)Globals::APIDefs->Log(ELogLevel_CRITICAL, "GW2TP", "Executable execution failed.");
    }
}

bool RenderUI::version_is_lower(const std::string current_version, const std::string &latest_version)
{
    return current_version < latest_version;
}

bool RenderUI::download_file(const std::string &url, const std::filesystem::path &outputPath)
{
    try
    {
        (void)Globals::APIDefs->Log(ELogLevel_DEBUG, "GW2TP", "Started Downloading");

        if (std::filesystem::exists(outputPath))
        {
            (void)Globals::APIDefs->Log(ELogLevel_INFO, "GW2TP", ("Deleting old file before download: " + outputPath.string()).c_str());
            std::filesystem::remove(outputPath);
        }

        const auto hr = URLDownloadToFileA(nullptr, url.c_str(), outputPath.string().c_str(), 0, nullptr);
        return SUCCEEDED(hr);
    }
    catch (...)
    {
        (void)Globals::APIDefs->Log(ELogLevel_CRITICAL, "GW2TP", "Downloading failed.");
        return false;
    }
}

bool RenderUI::extract_zip_file(const std::filesystem::path &zipPath, const std::filesystem::path &extractPath)
{
    try
    {
        (void)Globals::APIDefs->Log(ELogLevel_DEBUG, "GW2TP", "Started Extracting");

        const auto psCommand = "powershell.exe -Command \"Expand-Archive -Path '" + zipPath.string() +
                               "' -DestinationPath '" + extractPath.string() + "' -Force\"";

        STARTUPINFOA si = {sizeof(si)};
        PROCESS_INFORMATION pi = {};
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        if (CreateProcessA(nullptr,
                           const_cast<char *>(psCommand.c_str()),
                           nullptr,
                           nullptr,
                           FALSE,
                           0,
                           nullptr,
                           nullptr,
                           &si,
                           &pi))
        {
            (void)Globals::APIDefs->Log(ELogLevel_INFO, "GW2TP", "Started extraction process.");
            WaitForSingleObject(pi.hProcess, 30000);
            DWORD exitCode;
            GetExitCodeProcess(pi.hProcess, &exitCode);
            CloseHandle(pi.hProcess);
            CloseHandle(pi.hThread);

            if (exitCode != 0)
            {
                auto errorMsg = "Extraction failed with exit code: " + std::to_string(exitCode);
                (void)Globals::APIDefs->Log(ELogLevel_DEBUG, "GW2TP", errorMsg.c_str());
            }

            return exitCode == 0;
        }

        DWORD createProcessError = GetLastError();
        auto errorMsg = "Create Process Failed with error code: " + std::to_string(createProcessError);
        (void)Globals::APIDefs->Log(ELogLevel_CRITICAL, "GW2TP", errorMsg.c_str());
        return false;
    }
    catch (...)
    {
        (void)Globals::APIDefs->Log(ELogLevel_CRITICAL, "GW2TP", "Extraction failed.");
        return false;
    }
}

void RenderUI::download_and_extract_data_async(const std::filesystem::path &addonPath, const std::string &data_url, const std::string filename)
{
    std::thread([addonPath, data_url, filename]()
                {
        try
        {
            const auto extract_path = addonPath / filename;
            (void)Globals::APIDefs->Log(ELogLevel_DEBUG, "GW2TP", "Started Download Thread.");
            const auto download_name = "Downloading: " + data_url + ".";
            (void)Globals::APIDefs->Log(ELogLevel_DEBUG, "GW2TP", download_name.c_str());

            if (RenderUI::download_file(data_url, extract_path))
                Settings::Save(Globals::SettingsPath);
        }
        catch (...)
        {
            (void)Globals::APIDefs->Log(ELogLevel_DEBUG, "GW2TP", "DownloadAndExtractDataAsync failed.");
        } })
        .detach();
}

std::string RenderUI::get_clean_category_name(const std::string &input, bool skip_last_two)
{
    auto view = input | std::views::transform(
                            [newWord = true](char c) mutable
                            {
                                if (c == '_')
                                {
                                    newWord = true;
                                    return ' ';
                                }
                                if (newWord)
                                {
                                    newWord = false;
                                    return static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
                                }
                                return static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
                            });

    if (skip_last_two)
        return {view.begin(), view.end() - 2};
    else
        return {view.begin(), view.end()};
}

void RenderUI::open_url_in_browser(const std::string &url)
{
    if (!url.empty())
        ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

void RenderUI::render_last_update(char *time_text, size_t buffer_size, const std::chrono::steady_clock::time_point &last_refresh_time)
{
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_refresh_time);

    int total_seconds = static_cast<int>(elapsed.count());
    int hours = total_seconds / 3600;
    int minutes = (total_seconds % 3600) / 60;
    int seconds = total_seconds % 60;

    if (minutes < 1)
        snprintf(time_text, buffer_size, "Last update: %d seconds ago", seconds);
    else if (hours > 0)
        snprintf(time_text, buffer_size, "Last update: %02d:%02d:%02d ago", hours, minutes, seconds);
    else
        snprintf(time_text, buffer_size, "Last update: %02d:%02d ago", minutes, seconds);
}

void RenderUI::render_table_header(const std::string &name, const std::string &url)
{
    const auto transformed_name = get_clean_category_name(name, false);

    ImGui::TableSetupColumn(transformed_name.c_str(), ImGuiTableColumnFlags_WidthFixed, Render::NAME_COLUMN_WIDTH_PX);
    ImGui::TableSetupColumn("G", ImGuiTableColumnFlags_WidthFixed, Render::NUMBER_COLUMN_WIDTH_PX);
    ImGui::TableSetupColumn("S", ImGuiTableColumnFlags_WidthFixed, Render::NUMBER_COLUMN_WIDTH_PX);
    ImGui::TableSetupColumn("C", ImGuiTableColumnFlags_WidthFixed, Render::NUMBER_COLUMN_WIDTH_PX);
    ImGui::TableHeadersRow();

    if (ImGui::TableGetColumnFlags(0) & ImGuiTableColumnFlags_IsHovered && ImGui::IsMouseDoubleClicked(0))
    {
        if (!url.empty())
            open_url_in_browser(url);
        else
            ImGui::SetClipboardText(transformed_name.c_str());
    }
    if (ImGui::TableGetColumnFlags(1) & ImGuiTableColumnFlags_IsHovered && ImGui::IsMouseDoubleClicked(0))
    {
        if (!url.empty())
            open_url_in_browser(url);
        else
            ImGui::SetClipboardText(transformed_name.c_str());
    }
    if (ImGui::TableGetColumnFlags(2) & ImGuiTableColumnFlags_IsHovered && ImGui::IsMouseDoubleClicked(0))
    {
        if (!url.empty())
            open_url_in_browser(url);
        else
            ImGui::SetClipboardText(transformed_name.c_str());
    }
    if (ImGui::TableGetColumnFlags(3) & ImGuiTableColumnFlags_IsHovered && ImGui::IsMouseDoubleClicked(0))
    {
        if (!url.empty())
            open_url_in_browser(url);
        else
            ImGui::SetClipboardText(transformed_name.c_str());
    }
}

void RenderUI::render_profit_calculator()
{
    static char buy_gold_str[10] = "0";
    static char buy_silver_str[3] = "0";
    static char buy_copper_str[3] = "0";
    static char sell_gold_str[10] = "0";
    static char sell_silver_str[3] = "0";
    static char sell_copper_str[3] = "0";
    static int profit_gold = 0, profit_silver = 0, profit_copper = 0, profit_total = 0;

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

    center_next_element("Profit Calculator", true);
    ImGui::TextUnformatted("Profit Calculator");
    ImGui::Spacing();

    const float start_x = ImGui::GetCursorPosX();
    const float window_width = ImGui::GetWindowContentRegionWidth();
    const float total_width = 300.0f;
    ImGui::SetCursorPosX(start_x + (window_width - total_width) * 0.5f);

    ImGui::BeginGroup();
    ImGui::TextUnformatted("Buy Price:");
    ImGui::SameLine();
    ImGui::PushItemWidth(60.0f);
    ImGui::InputText("##buy-g", buy_gold_str, sizeof(buy_gold_str), ImGuiInputTextFlags_CharsDecimal);
    ImGui::SameLine();
    ImGui::TextUnformatted("g");
    ImGui::SameLine();
    ImGui::InputText("##buy-s", buy_silver_str, sizeof(buy_silver_str), ImGuiInputTextFlags_CharsDecimal);
    ImGui::SameLine();
    ImGui::TextUnformatted("s");
    ImGui::SameLine();
    ImGui::InputText("##buy-c", buy_copper_str, sizeof(buy_copper_str), ImGuiInputTextFlags_CharsDecimal);
    ImGui::SameLine();
    ImGui::TextUnformatted("c");
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    ImGui::SetCursorPosX(start_x + (window_width - total_width) * 0.5f);
    ImGui::BeginGroup();
    ImGui::TextUnformatted("Sell Price:");
    ImGui::SameLine();
    ImGui::PushItemWidth(60.0f);
    ImGui::InputText("##sell-g", sell_gold_str, sizeof(sell_gold_str), ImGuiInputTextFlags_CharsDecimal);
    ImGui::SameLine();
    ImGui::TextUnformatted("g");
    ImGui::SameLine();
    ImGui::InputText("##sell-s", sell_silver_str, sizeof(sell_silver_str), ImGuiInputTextFlags_CharsDecimal);
    ImGui::SameLine();
    ImGui::TextUnformatted("s");
    ImGui::SameLine();
    ImGui::InputText("##sell-c", sell_copper_str, sizeof(sell_copper_str), ImGuiInputTextFlags_CharsDecimal);
    ImGui::SameLine();
    ImGui::TextUnformatted("c");
    ImGui::PopItemWidth();
    ImGui::EndGroup();

    const float child_width = 300.0f;
    ImGui::SetCursorPosX((window_width - child_width) * 0.5f);
    ImGui::BeginChild("calcButtonAndResult", ImVec2(child_width, 50));
    const float button_width = 100.0f;
    if (ImGui::Button("Calculate", ImVec2(button_width, 0)))
    {
        (void)Globals::APIDefs->Log(ELogLevel_DEBUG, "GW2TP", "Calculating profit...");

        const auto buy_gold = atoi(buy_gold_str);
        const auto buy_silver = atoi(buy_silver_str);
        const auto buy_copper = atoi(buy_copper_str);
        const auto sell_gold = atoi(sell_gold_str);
        const auto sell_silver = atoi(sell_silver_str);
        const auto sell_copper = atoi(sell_copper_str);

        const auto buy_total = (buy_gold * 10000) + (buy_silver * 100) + buy_copper;
        const auto sell_total = (sell_gold * 10000) + (sell_silver * 100) + sell_copper;
        profit_total = static_cast<int>(sell_total * TAX_RATE - buy_total);
        profit_gold = static_cast<int>(profit_total / 10000);
        profit_total %= 10000;
        profit_silver = static_cast<int>(profit_total / 100);
        profit_copper = static_cast<int>(profit_total % 100);
    }

    char profit_text[50];
    snprintf(profit_text, sizeof(profit_text), "Profit: %dg %ds %dc", profit_gold, profit_silver, profit_copper);
    ImGui::SameLine();
    if (profit_total > 0)
        ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f), "%s", profit_text);
    else if (profit_total < 0)
        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", profit_text);
    else
        ImGui::TextUnformatted(profit_text);

    ImGui::PopStyleVar();
    ImGui::Spacing();
    ImGui::EndChild();
}

void RenderUI::render_top_controls(Data &data, std::chrono::steady_clock::time_point &last_refresh_time)
{
    static auto started_gw2tp_download = false;
    static auto started_forge_download = false;
    static auto started_clicker_download = false;

    bool has_gw2tp_files = false;
    bool has_forge_files = false;
    bool has_clicker_files = false;

    bool is_outdated_gw2tp = false;
    bool is_outdated_forge = false;
    bool is_outdated_clicker = false;

    auto backend_exe = Globals::AddonPath / "GW2TP_Python.exe";
    has_gw2tp_files = std::filesystem::exists(backend_exe);
    auto forge_code_dir = Globals::AddonPath / "GW2_Forge.exe";
    has_forge_files = std::filesystem::exists(forge_code_dir);
    auto clicker_exe = Globals::AddonPath / "GW2_AutoClicker.exe";
    has_clicker_files = std::filesystem::exists(clicker_exe);

    if (has_gw2tp_files && !started_gw2tp_download && VersionIsLower(Settings::BackendVersion, latest_backend_version))
    {
        is_outdated_gw2tp = true;
        (void)Globals::APIDefs->Log(ELogLevel_INFO, "GW2TP", "Outdated backend files detected.");
    }

    if (has_forge_files && !started_forge_download && VersionIsLower(Settings::ForgeVersion, latest_forge_version))
    {
        is_outdated_forge = true;
        (void)Globals::APIDefs->Log(ELogLevel_INFO, "GW2TP", "Outdated forge files detected.");
    }

    if (has_clicker_files && !started_clicker_download && VersionIsLower(Settings::ClickerVersion, latest_clicker_version))
    {
        is_outdated_clicker = true;
        (void)Globals::APIDefs->Log(ELogLevel_INFO, "GW2TP", "Outdated clicker files detected.");
    }

    if ((!has_gw2tp_files || is_outdated_gw2tp) && !started_gw2tp_download)
    {
        started_gw2tp_download = true;
        Settings::BackendVersion = latest_backend_version;
        DownloadAndExtractDataAsync(Globals::AddonPath, backend_url, "GW2TP_Python.exe");
    }

    if ((!has_forge_files || is_outdated_forge) && !started_forge_download)
    {
        started_forge_download = true;
        Settings::ForgeVersion = latest_forge_version;
        DownloadAndExtractDataAsync(Globals::AddonPath, forge_url, "GW2_Forge.exe");
    }

    if ((!has_clicker_files || is_outdated_clicker) && !started_clicker_download)
    {
        started_clicker_download = true;
        Settings::ClickerVersion = latest_clicker_version;
        DownloadAndExtractDataAsync(Globals::AddonPath, clicker_url, "GW2_AutoClicker.exe");
    }

    static bool show_forge_cmd = true;
    static bool show_clicker_cmd = true;
    static int num_forges = 0;
    static int num_clicks = 1;

    const auto *forge_button_label = "Start Forge Script";
    const auto *refresh_button_label = "Refresh Data";
    const auto *loading_label = "Loading...";
    const auto *clicker_button_label = "Start Auto Clicker";
    const auto *update_button_label = "Update Scripts";
    char time_text[50];
    RenderUI::render_last_update(time_text, sizeof(time_text), last_refresh_time);

    const auto input_width = 100.0f + ImGui::CalcTextSize("Num forges").x + ImGui::GetStyle().ItemInnerSpacing.x;
    const auto clicker_input_width = 60.0f + ImGui::CalcTextSize("Clicks").x + ImGui::GetStyle().ItemInnerSpacing.x;
    const auto forge_btn_width = ImGui::CalcTextSize(forge_button_label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const auto refresh_btn_width = ImGui::CalcTextSize(refresh_button_label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const auto clicker_btn_width = ImGui::CalcTextSize(clicker_button_label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const auto update_btn_width = ImGui::CalcTextSize(update_button_label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    const auto loading_width = ImGui::CalcTextSize(loading_label).x;
    const auto spacing = ImGui::GetStyle().ItemSpacing.x;
    const auto update_text_width = ImGui::CalcTextSize(time_text).x + ImGui::GetStyle().FramePadding.x * 2.0f;

    const auto first_row_width = refresh_btn_width + spacing + update_btn_width + spacing + update_text_width;
    const auto second_row_width = input_width + spacing + forge_btn_width + spacing + clicker_input_width + spacing + clicker_btn_width;

    center_group_with_width(first_row_width);

    if (ImGui::Button(update_button_label))
    {
        RenderUI::download_and_extract_data_async(Globals::AddonPath, forge_url, "GW2_Forge.exe");
        RenderUI::download_and_extract_data_async(Globals::AddonPath, clicker_url, "GW2_AutoClicker.exe");
        RenderUI::download_and_extract_data_async(Globals::AddonPath, backend_url, "GW2TP_Python.exe");
    }

    ImGui::SameLine();

    if (data.loaded)
    {
        if (ImGui::Button(refresh_button_label))
        {
            data.loaded = false;
            data.requested = false;
            data.api_data.clear();
            data.futures.clear();
            data.requesting();
            last_refresh_time = std::chrono::steady_clock::now();
        }
    }
    else
    {
        ImGui::TextUnformatted(loading_label);
    }

    ImGui::SameLine();
    ImGui::TextUnformatted(time_text);

    ImGui::Spacing();
    center_group_with_width(second_row_width);
    ImGui::SetNextItemWidth(100.0f);
    ImGui::InputInt("Num forges", &num_forges, 1, 250);

    ImGui::SameLine();
    if (ImGui::Button(forge_button_label))
    {
        const auto forge_exe_path = (Globals::AddonPath / "GW2_Forge.exe").string();
        const auto forge_args = "-n " + std::to_string(num_forges);

#ifdef _DEBUG
        RenderUI::start_executable(forge_exe_path, forge_args, true);
#else
        RenderUI::start_executable(forge_exe_path, forge_args, false);
#endif
    }

    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::InputInt("Clicks", &num_clicks, 1, 1000);

    ImGui::SameLine();
    if (ImGui::Button(clicker_button_label))
    {
        const auto clicker_exe_path = (Globals::AddonPath / "GW2_AutoClicker.exe").string();
        const auto clicker_args = std::to_string(num_clicks);
        RenderUI::start_executable(clicker_exe_path, clicker_args, show_clicker_cmd);
    }
}
