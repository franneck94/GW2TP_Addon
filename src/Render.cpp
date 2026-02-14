#include <windows.h>

#include <shellapi.h>
#include <shlwapi.h>
#include <urlmon.h>
#include <wininet.h>

#pragma comment(lib, "wininet.lib")
#pragma comment(lib, "urlmon.lib")
#pragma comment(lib, "shlwapi.lib")

#include <DirectXMath.h>
#include <Windows.h>
#include <d3d11.h>
#include <dxgi.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <map>
#include <mutex>
#include <ranges>
#include <string>
#include <vector>

#include "httpclient/httpclient.h"
#include "imgui.h"

#include "API.h"
#include "Constants.h"
#include "Data.h"
#include "Render.h"
#include "Shared.h"

namespace
{

    bool DownloadFile(const std::string &url, const std::filesystem::path &outputPath)
    {
        try
        {
            (void)APIDefs->Log(ELogLevel_DEBUG, "GW2TP", "Started ZIP Downloading");

            const auto hr = URLDownloadToFileA(nullptr, url.c_str(), outputPath.string().c_str(), 0, nullptr);
            return SUCCEEDED(hr);
        }
        catch (...)
        {
            (void)APIDefs->Log(ELogLevel_CRITICAL, "GW2TP", "ZIP downloading failed.");
            return false;
        }
    }

    bool ExtractZipFile(const std::filesystem::path &zipPath, const std::filesystem::path &extractPath)
    {
        try
        {
            (void)APIDefs->Log(ELogLevel_DEBUG, "GW2TP", "Started ZIP Extracting");

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
                (void)APIDefs->Log(ELogLevel_INFO, "GW2TP", "Started ZIP extraction process.");
                WaitForSingleObject(pi.hProcess, 30000); // Wait max 30 seconds
                DWORD exitCode;
                GetExitCodeProcess(pi.hProcess, &exitCode);
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);

                if (exitCode != 0)
                {
                    auto errorMsg = "ZIP extraction failed with exit code: " + std::to_string(exitCode);
                    (void)APIDefs->Log(ELogLevel_DEBUG, "GW2TP", errorMsg.c_str());
                }

                return exitCode == 0;
            }

            DWORD createProcessError = GetLastError();
            auto errorMsg = "Create Process Failed with error code: " + std::to_string(createProcessError);
            (void)APIDefs->Log(ELogLevel_CRITICAL, "GW2TP", errorMsg.c_str());
            return false;
        }
        catch (...)
        {
            (void)APIDefs->Log(ELogLevel_CRITICAL, "GW2TP", "ZIP extraction failed.");
            return false;
        }
    }

    void DownloadAndExtractDataAsync(const std::filesystem::path &addonPath, const std::string &data_url, const std::string dir_name)
    {
        std::thread([addonPath, data_url, dir_name]()
                    {
        try
        {
            auto temp_zip_path = addonPath / ("temp" + dir_name + ".zip");
            auto extract_path = addonPath / dir_name;
            (void)APIDefs->Log(ELogLevel_DEBUG, "GW2TP", "Started Download Thread.");

            if (DownloadFile(data_url, temp_zip_path))
            {
                std::filesystem::create_directories(addonPath);

                if (ExtractZipFile(temp_zip_path, extract_path))
                {
                    std::filesystem::remove(temp_zip_path);
                }
                else
                {
                    std::filesystem::remove(temp_zip_path);
                }
            }
        }
        catch (...)
        {
            (void)APIDefs->Log(ELogLevel_DEBUG, "GW2TP", "DownloadAndExtractDataAsync failed.");
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

                (void)APIDefs->Log(ELogLevel_INFO,
                                   "GW2TP",
                                   ("Removed old build files from " + path.string() + " directory").c_str());
            }
        }
        catch (const std::filesystem::filesystem_error &e)
        {
            auto errorMsg = "Error removing old builds from " + path.string() + " directory: " + std::string(e.what());
            (void)APIDefs->Log(ELogLevel_WARNING, "GW2TP", errorMsg.c_str());
        }
        catch (...)
        {
            (void)APIDefs->Log(
                ELogLevel_WARNING,
                "GW2TP",
                ("Unknown error while removing old builds from " + path.string() + " directory").c_str());
        }
    }

    std::string get_url_for_request_id(const std::string &request_id)
    {
        static const std::map<std::string, std::string> url_map = {
            {"rare_gear_salvage", "https://wiki.guildwars2.com/wiki/Piece_of_Rare_Unidentified_Gear/Salvage_Rate"},
            {"gear_salvage", "https://wiki.guildwars2.com/wiki/Piece_of_Unidentified_Gear/Salvage_Rate"},
            {"common_gear_salvage", "https://wiki.guildwars2.com/wiki/Piece_of_Common_Unidentified_Gear/Salvage_Rate"},
            {"lodestone_forge", "https://fast.farming-community.eu/conversions/spirit-shard"},
            {"charm_brilliance_forge", "https://fast.farming-community.eu/conversions/spirit-shard/charm-of-brilliance"},
            {"rare_weapon_craft", "https://wiki.guildwars2.com/wiki/Krait_Shell"},
            {"thesis_on_masterful_malice", "https://wiki.guildwars2.com/wiki/Thesis_on_Masterful_Malice"},
            {"symbol_enh_forge", "https://fast.farming-community.eu/conversions/spirit-shard/symbol-of-enhancement"},
            {"scholar_rune", "https://wiki.guildwars2.com/wiki/Superior_Rune_of_the_Scholar"},
            {"guardian_rune", "https://wiki.guildwars2.com/wiki/Superior_Rune_of_the_Guardian"},
            {"dragonhunter_rune", "https://wiki.guildwars2.com/wiki/Superior_Rune_of_the_Dragonhunter"},
            {"relic_of_fireworks", "https://wiki.guildwars2.com/wiki/Relic_of_Fireworks"},
            {"relic_of_thief", "https://wiki.guildwars2.com/wiki/Relic_of_the_Thief"},
            {"relic_of_aristocracy", "https://wiki.guildwars2.com/wiki/Relic_of_the_Aristocracy"},
            {"hard_leather_strap", "https://fast.farming-community.eu/conversions/spirit-shard/charm-of-brilliance"},
            {"sigil_of_impact", "https://wiki.guildwars2.com/wiki/Superior_Sigil_of_Impact"},
            {"sigil_of_doom", "https://wiki.guildwars2.com/wiki/Superior_Sigil_of_Doom"},
            {"sigil_of_torment", "https://wiki.guildwars2.com/wiki/Superior_Sigil_of_Torment"},
            {"sigil_of_bursting", "https://wiki.guildwars2.com/wiki/Superior_Sigil_of_Bursting"},
            {"sigil_of_paralyzation", "https://wiki.guildwars2.com/wiki/Superior_Sigil_of_Paralyzation"}};

        auto it = url_map.find(request_id);
        return (it != url_map.end()) ? it->second : "";
    }

    void open_url_in_browser(const std::string &url)
    {
        if (!url.empty())
        {
            ShellExecuteA(nullptr, "open", url.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
    }

    void start_python_script(const std::string &script_path, const std::string &args = "", bool show_cmd_window = false)
    {
        try
        {
            std::string command = "cmd /k python \"" + script_path + "\"";
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
                (void)APIDefs->Log(ELogLevel_INFO, "GW2TP", ("Started Python script: " + script_path).c_str());
                CloseHandle(pi.hProcess);
                CloseHandle(pi.hThread);
            }
            else
            {
                DWORD createProcessError = GetLastError();
                auto errorMsg = "Failed to start Python script with error code: " + std::to_string(createProcessError);
                (void)APIDefs->Log(ELogLevel_CRITICAL, "GW2TP", errorMsg.c_str());
            }
        }
        catch (...)
        {
            (void)APIDefs->Log(ELogLevel_CRITICAL, "GW2TP", "Python script execution failed.");
        }
    }

    std::string get_clean_category_name(const std::string &input, const bool skip_last_two)
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

    void remove_substring(std::string &str, const std::string &sub)
    {
        size_t pos;
        while ((pos = str.find(sub)) != std::string::npos)
        {
            str.erase(pos, sub.size());
        }
    }

    void add_row(const std::string name, const Price &price)
    {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::Text(name.c_str());
        ImGui::TableNextColumn();
        ImGui::Text("%d", price.gold);
        ImGui::TableNextColumn();
        ImGui::Text("%d", price.silver);
        ImGui::TableNextColumn();
        ImGui::Text("%d", price.copper);
    }

    void add_header(const std::string name, const std::string &url = "")
    {
        const auto transformed_name = get_clean_category_name(name, false);

        ImGui::TableSetupColumn(transformed_name.c_str(), ImGuiTableColumnFlags_WidthFixed, Render::NAME_COLUMN_WIDTH_PX);
        ImGui::TableSetupColumn("G", ImGuiTableColumnFlags_WidthFixed, Render::NUMBER_COLUMN_WIDTH_PX);
        ImGui::TableSetupColumn("S", ImGuiTableColumnFlags_WidthFixed, Render::NUMBER_COLUMN_WIDTH_PX);
        ImGui::TableSetupColumn("C", ImGuiTableColumnFlags_WidthFixed, Render::NUMBER_COLUMN_WIDTH_PX);
        ImGui::TableHeadersRow();

        // Check for hover and double-click after headers are rendered
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

    template <size_t N>
    void _get_ordered_row_data(const std::array<const char *, N> &keys, const std::vector<std::pair<std::string, Price>> &rows)
    {
        for (const auto &key : keys)
        {
            auto it = std::find_if(rows.begin(), rows.end(), [&key](const auto &pair)
                                   { return pair.first == key; });
            if (it != rows.end())
            {
                const auto name = get_clean_category_name(it->first, false);
                const auto price = it->second;

                add_row(name, price);
            }
        }
    }

    void get_row_data(const std::map<std::string, int> &kv, const std::string &request_id)
    {
        auto rows = std::vector<std::pair<std::string, Price>>{};

        if (kv.size() < 3)
            return;

        for (size_t i = 0; i + 2 < kv.size(); i += 3)
        {
            auto it = std::next(kv.begin(), i);

            if (it->first.find("_g") == std::string::npos && it->first.find("_s") == std::string::npos && it->first.find("_c") == std::string::npos)
            {
                ++i;
                ++it;
            }

            if (std::distance(it, kv.end()) < 3)
                break;

            auto name0 = std::next(it, 0)->first;
            auto name1 = std::next(it, 1)->first;
            auto name2 = std::next(it, 2)->first;

            const auto val0 = std::next(it, 0)->second;
            const auto val1 = std::next(it, 1)->second;
            const auto val2 = std::next(it, 2)->second;

            const auto gold = name0.ends_with("_g") ? val0 : name1.ends_with("_g") ? val1
                                                                                   : val2;
            const auto silver = name0.ends_with("_s") ? val0 : name1.ends_with("_s") ? val1
                                                                                     : val2;
            const auto copper = name0.ends_with("_c") ? val0 : name1.ends_with("_c") ? val1
                                                                                     : val2;

            const auto transformed_name = std::string{name0.substr(0, name0.size() - 2)};

            rows.emplace_back(transformed_name, Price{
                                                    .copper = copper,
                                                    .silver = silver,
                                                    .gold = gold,
                                                });
        }

        // other
        if (request_id == "thesis_on_masterful_malice")
            _get_ordered_row_data(API::THESIS_MASTERFUL_MALICE, rows);
        // runes
        else if (request_id == "scholar_rune")
            _get_ordered_row_data(API::SCHOLAR_RUNE_NAMES, rows);
        else if (request_id == "dragonhunter_rune")
            _get_ordered_row_data(API::DRAGONHUNTER_RUNE_NAMES, rows);
        else if (request_id == "guardian_rune")
            _get_ordered_row_data(API::GUARDIAN_RUNE_NAMES, rows);
        // relics
        else if (request_id == "relic_of_fireworks")
            _get_ordered_row_data(API::FIREWORKS_NAMES, rows);
        else if (request_id == "relic_of_thief")
            _get_ordered_row_data(API::THIEF_NAMES, rows);
        else if (request_id == "relic_of_aristocracy")
            _get_ordered_row_data(API::ARISTOCRACY_NAMES, rows);
        // sigil
        else if (request_id == "sigil_of_impact")
            _get_ordered_row_data(API::SIGIL_OF_IMPACT_NAMES, rows);
        else if (request_id == "sigil_of_torment")
            _get_ordered_row_data(API::SIGIL_OF_TORMENT_NAMES, rows);
        else if (request_id == "sigil_of_doom")
            _get_ordered_row_data(API::SIGIL_OF_DOOM_NAMES, rows);
        else if (request_id == "sigil_of_bursting")
            _get_ordered_row_data(API::SIGIL_OF_BURSTING_NAMES, rows);
        else if (request_id == "sigil_of_paralyzation")
            _get_ordered_row_data(API::SIGIL_OF_PARALYZATION_NAMES, rows);
        // gear
        else if (request_id == "rare_weapon_craft")
            _get_ordered_row_data(API::RARE_WEAPON_CRAFT_NAMES, rows);
        else if (request_id == "rare_gear_salvage")
            _get_ordered_row_data(API::RARE_GEAR_NAMES, rows);
        // gear
        else if (request_id == "gear_salvage")
            _get_ordered_row_data(API::GEAR_SALVAGE_NAMES, rows);
        else if (request_id == "common_gear_salvage")
            _get_ordered_row_data(API::COMMON_GEAR_NAMES, rows);
        // t5
        else if (request_id == "t5_mats_buy")
            _get_ordered_row_data(API::T5_MATS_BUY_NAMES, rows);
        else if (request_id == "mats_crafting_compare")
            _get_ordered_row_data(API::MATS_CRAFTING_COMPARE_NAMES, rows);
        // forge
        else if (request_id == "symbol_enh_forge")
            _get_ordered_row_data(API::FORGE_ENH_NAMES, rows);
        else if (request_id == "charm_brilliance_forge")
            _get_ordered_row_data(API::FORGE_CHARM_NAMES, rows);
        else if (request_id == "lodestone_forge")
            _get_ordered_row_data(API::LODESTONE_NAMES, rows);
        else if (request_id == "ecto" || request_id == "rare_gear")
        {
            for (const auto &[name, price] : rows)
                add_row(name, price);
        }
        else
            int i = 2;
    }

    void center_next_element(const char *label, bool is_text = false)
    {
        float windowWidth = ImGui::GetWindowSize().x;
        float elementWidth;
        if (is_text)
        {
            elementWidth = ImGui::CalcTextSize(label).x;
        }
        else
        {
            elementWidth = ImGui::CalcTextSize(label).x + ImGui::GetStyle().FramePadding.x * 2.0f;
        }
        ImGui::SetCursorPosX((windowWidth - elementWidth) * 0.5f);
    }

    void center_next_checkbox(const char *label)
    {
        const float windowWidth = ImGui::GetWindowSize().x;
        const float checkboxWidth = ImGui::GetFrameHeight();
        const float textWidth = ImGui::CalcTextSize(label).x;
        const float innerSpacing = ImGui::GetStyle().ItemInnerSpacing.x;
        const float totalWidth = checkboxWidth + innerSpacing + textWidth;
        ImGui::SetCursorPosX((windowWidth - totalWidth) * 0.5f);
    }

    void render_profit_calculator()
    {
        static char buy_gold_str[10] = "0";
        static char buy_silver_str[3] = "0";
        static char buy_copper_str[3] = "0";
        static char sell_gold_str[10] = "0";
        static char sell_silver_str[3] = "0";
        static char sell_copper_str[3] = "0";
        static int profit_gold = 0, profit_silver = 0, profit_copper = 0, profit_total = 0;

        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4.0f, 4.0f));

        // Title
        center_next_element("Profit Calculator", true);
        ImGui::Text("Profit Calculator");
        ImGui::Spacing();

        // Buy Price Input
        float start_x = ImGui::GetCursorPosX();
        float window_width = ImGui::GetWindowContentRegionWidth();
        float total_width = 300.0f;
        ImGui::SetCursorPosX(start_x + (window_width - total_width) * 0.5f);

        ImGui::BeginGroup();
        ImGui::Text("Buy Price:");
        ImGui::SameLine();
        ImGui::PushItemWidth(60.0f);
        ImGui::InputText("##buy-g", buy_gold_str, sizeof(buy_gold_str), ImGuiInputTextFlags_CharsDecimal);
        ImGui::SameLine();
        ImGui::Text("g");
        ImGui::SameLine();
        ImGui::InputText("##buy-s", buy_silver_str, sizeof(buy_silver_str), ImGuiInputTextFlags_CharsDecimal);
        ImGui::SameLine();
        ImGui::Text("s");
        ImGui::SameLine();
        ImGui::InputText("##buy-c", buy_copper_str, sizeof(buy_copper_str), ImGuiInputTextFlags_CharsDecimal);
        ImGui::SameLine();
        ImGui::Text("c");
        ImGui::PopItemWidth();
        ImGui::EndGroup();

        ImGui::SetCursorPosX(start_x + (window_width - total_width) * 0.5f);
        ImGui::BeginGroup();
        ImGui::Text("Sell Price:");
        ImGui::SameLine();
        ImGui::PushItemWidth(60.0f);
        ImGui::InputText("##sell-g", sell_gold_str, sizeof(sell_gold_str), ImGuiInputTextFlags_CharsDecimal);
        ImGui::SameLine();
        ImGui::Text("g");
        ImGui::SameLine();
        ImGui::InputText("##sell-s", sell_silver_str, sizeof(sell_silver_str), ImGuiInputTextFlags_CharsDecimal);
        ImGui::SameLine();
        ImGui::Text("s");
        ImGui::SameLine();
        ImGui::InputText("##sell-c", sell_copper_str, sizeof(sell_copper_str), ImGuiInputTextFlags_CharsDecimal);
        ImGui::SameLine();
        ImGui::Text("c");
        ImGui::PopItemWidth();
        ImGui::EndGroup();

        float child_width = 300.0f;
        ImGui::SetCursorPosX((window_width - child_width) * 0.5f);
        ImGui::BeginChild("calcButtonAndResult", ImVec2(child_width, 50));
        float button_width = 100.0f;
        if (ImGui::Button("Calculate", ImVec2(button_width, 0)))
        {
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
            ImGui::TextColored(ImVec4(1.0f, 0.843f, 0.0f, 1.0f), "%s", profit_text); // Gold color
        else if (profit_total < 0)
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "%s", profit_text); // Red color
        else
            ImGui::Text("%s", profit_text);

        ImGui::PopStyleVar();
        ImGui::Spacing();
        ImGui::EndChild();
    }

    void UpdateTimer(const std::chrono::steady_clock::time_point &last_refresh_time)
    {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - last_refresh_time);

        int total_seconds = static_cast<int>(elapsed.count());
        int hours = total_seconds / 3600;
        int minutes = (total_seconds % 3600) / 60;
        int seconds = total_seconds % 60;

        char time_text[50];
        if (minutes < 1)
            snprintf(time_text, sizeof(time_text), "Last update: %d seconds ago", seconds);
        else if (hours > 0)
            snprintf(time_text, sizeof(time_text), "Last update: %02d:%02d:%02d ago", hours, minutes, seconds);
        else
            snprintf(time_text, sizeof(time_text), "Last update: %02d:%02d ago", minutes, seconds);
        center_next_element(time_text, true);
        ImGui::Text("%s", time_text);
    }
}

int Render::render_table(const std::string &request_id)
{
    const auto &kv = data.api_data[request_id];
    if (API::COMMANDS.find(request_id) == API::COMMANDS.end() || data.api_data.find(request_id) == data.api_data.end() || data.api_data[request_id].empty())
    {
        ImGui::Text("No data yet for %s", request_id.c_str());

        return -1;
    }

    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableColumnFlags_NoSort;
    if (ImGui::BeginTable(("Prices##" + request_id).c_str(), 4, flags))
    {
        const std::string url = get_url_for_request_id(request_id);
        add_header(request_id, url);

        if (API::COMMANDS.find(request_id) != API::COMMANDS.end())
            get_row_data(kv, request_id);

        ImGui::EndTable();
    }

    return static_cast<int>(ImGui::GetCursorPosY());
}

void Render::top_section_child()
{
    static auto last_refresh_time = std::chrono::steady_clock::now();
    static bool first_load = true;

    const auto window_width = ImGui::GetWindowContentRegionWidth();

    ImGui::BeginChild("TopSection", ImVec2(window_width, 160.0f), false, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    // Center the button group
    auto *checkbox_label1 = "Start Forge Script";
    auto *checkbox_label2 = "Start Localhost Server";
    auto *checkbox_label3 = "Use localhost server";

    static bool show_forge_cmd = true;
    static bool show_server_cmd = false;

    float button1_width = ImGui::CalcTextSize(checkbox_label1).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    float button2_width = ImGui::CalcTextSize(checkbox_label2).x + ImGui::GetStyle().FramePadding.x * 2.0f;
    float checkbox_width = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize(checkbox_label3).x;
    float spacing = ImGui::GetStyle().ItemSpacing.x;
    float total_width = button1_width + spacing + button2_width + spacing + checkbox_width;

    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - total_width) * 0.5f);

    if (ImGui::Button(checkbox_label1))
    {
        auto forge_script_path = (AddonPath / "GW2_Forge" / "GW2MysticForge-0.1.0" / "main.py").string();
        start_python_script(forge_script_path, "", show_forge_cmd);
    }

    ImGui::SameLine();

    if (ImGui::Button(checkbox_label2))
    {
        auto server_script_path = (AddonPath / "GW2TP_Python" / "Gw2TP-1.0.0" / "server.py").string();
        start_python_script(server_script_path, "", show_server_cmd);
    }
    ImGui::SameLine();

    if (ImGui::Checkbox(checkbox_label3, &data.use_localhost))
    {
        data.loaded = false;
        data.requested = false;
        data.api_data.clear();
        data.futures.clear();
        data.requesting();
        last_refresh_time = std::chrono::steady_clock::now();
        first_load = false;
    }

    // Add checkboxes for command window visibility
    ImGui::Spacing();

    // Center the cmd window checkboxes
    float cmd_checkbox1_width = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize("Show Forge CMD").x;
    float cmd_checkbox2_width = ImGui::GetFrameHeight() + ImGui::GetStyle().ItemInnerSpacing.x + ImGui::CalcTextSize("Show Server CMD").x;
    float cmd_total_width = cmd_checkbox1_width + spacing + cmd_checkbox2_width;

    ImGui::SetCursorPosX((ImGui::GetWindowSize().x - cmd_total_width) * 0.5f);

    ImGui::Checkbox("Show Forge CMD", &show_forge_cmd);
    ImGui::SameLine();
    ImGui::Checkbox("Show Server CMD", &show_server_cmd);

    auto *btn_label = "Refresh Data";
    center_next_element(btn_label);
    if (!data.loaded)
    {
        auto *text_label = "Loading...";
        center_next_element(text_label, true);
        ImGui::Text(text_label);
    }
    else
    {
        if (ImGui::Button(btn_label))
        {
            data.loaded = false;
            data.requested = false;
            data.api_data.clear();
            data.futures.clear();
            data.requesting();
            last_refresh_time = std::chrono::steady_clock::now();
            first_load = false;
        }
    }

    UpdateTimer(last_refresh_time);

    ImGui::Spacing();
    ImGui::Separator();
    render_profit_calculator();
    ImGui::Separator();

    ImGui::EndChild();
}

void Render::render_tables_for_commands(const std::set<std::string> &commands, uint32_t &idx)
{
    const auto window_width = ImGui::GetWindowContentRegionWidth();
    const auto large_window = window_width > 450.0F;
    const auto very_large_window = window_width > 750.0F;
    const auto child_size = ImVec2(window_width * (very_large_window ? 0.33f : (large_window ? 0.5f : 1.0F)), TABLE_HEIGHT_PX);

    for (const auto command : commands)
    {
        ImGui::BeginChild(("tableChild" + std::to_string(idx)).c_str(), child_size, false, ImGuiWindowFlags_AlwaysAutoResize);
        const auto table_height = render_table(command);
        ImGui::EndChild();
        if (very_large_window && (idx % 3 != 2))
            ImGui::SameLine();
        else if (!very_large_window && large_window && (idx % 2 == 0))
            ImGui::SameLine();
        ++idx;
    }
}

void Render::table_child()
{
    const auto window_width = ImGui::GetWindowContentRegionWidth();

    ImGui::BeginChild("ScrollableContent", ImVec2(window_width, -1.0), false, ImGuiWindowFlags_AlwaysAutoResize);

    auto idx = 0U;
    render_tables_for_commands(API::OTHER_COMMANDS, idx);
    render_tables_for_commands(API::GEAR_COMMANDS, idx);
    render_tables_for_commands(API::FORGE_COMMANDS, idx);
    render_tables_for_commands(API::RUNE_COMMANDS, idx);
    render_tables_for_commands(API::SIGIL_COMMANDS, idx);
    render_tables_for_commands(API::RELIC_COMMANDS, idx);

    ImGui::EndChild();
}

void Render::render()
{
    static auto started_gw2tp_download = false;
    static auto started_forge_download = false;
    bool has_gw2tp_files = false;
    bool has_forge_files = false;

    if (!show_window)
        return;

    auto python_code_dir = AddonPath / "GW2TP_Python";
    if (std::filesystem::exists(python_code_dir))
    {
        try
        {
            has_gw2tp_files = !std::filesystem::is_empty(python_code_dir);
        }
        catch (...)
        {
            has_gw2tp_files = false;
        }
    }

    if (!has_gw2tp_files && !started_gw2tp_download)
    {
        const std::string data_url =
            "https://github.com/franneck94/Gw2TP/archive/refs/tags/1.0.0.zip";

        DownloadAndExtractDataAsync(AddonPath, data_url, "GW2TP_Python");
        started_gw2tp_download = true;
    }

    auto forge_code_dir = AddonPath / "GW2_Forge";
    if (std::filesystem::exists(forge_code_dir))
    {
        try
        {
            has_forge_files = !std::filesystem::is_empty(forge_code_dir);
        }
        catch (...)
        {
            has_forge_files = false;
        }
    }

    if (!has_forge_files && !started_forge_download)
    {
        const std::string data_url =
            "https://github.com/franneck94/GW2MysticForge/archive/refs/tags/0.1.0.zip";

        DownloadAndExtractDataAsync(AddonPath, data_url, "GW2_Forge");
        started_forge_download = true;
    }

    if (ImGui::Begin("GW2TP", &show_window))
    {
        top_section_child();
        table_child();
    }

    ImGui::End();
}
