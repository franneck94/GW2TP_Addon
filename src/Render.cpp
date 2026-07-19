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

#include <atomic>
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
#include <thread>
#include <vector>

#include "httpclient/httpclient.h"
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
    std::string get_url_for_request_id(const std::string &request_id)
    {
        static const std::map<std::string, std::string> url_map = {
            {"rare_gear_salvage", "https://wiki.guildwars2.com/wiki/Piece_of_Rare_Unidentified_Gear/Salvage_Rate"},
            {"gear_salvage", "https://wiki.guildwars2.com/wiki/Piece_of_Unidentified_Gear/Salvage_Rate"},
            {"common_gear_salvage", "https://wiki.guildwars2.com/wiki/Piece_of_Common_Unidentified_Gear/Salvage_Rate"},
            {"lodestone_forge", "https://fast.farming-community.eu/conversions/spirit-shard"},
            {"charm_brilliance_forge", "https://fast.farming-community.eu/conversions/spirit-shard/charm-of-brilliance"},
            {"krait_shield_craft", "https://wiki.guildwars2.com/wiki/Krait_Shell"},
            {"krait_trident_craft", "https://wiki.guildwars2.com/wiki/Krait_Trident"},
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
        else if (request_id == "krait_shield_craft")
            _get_ordered_row_data(API::KRAIT_SHIELD_CRAFT_NAMES, rows);
        else if (request_id == "krait_trident_craft")
            _get_ordered_row_data(API::KRAIT_TRIDENT_CRAFT_NAMES, rows);
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
        RenderUI::render_table_header(request_id, url);

        if (API::COMMANDS.find(request_id) != API::COMMANDS.end())
            get_row_data(kv, request_id);

        ImGui::EndTable();
    }

    return static_cast<int>(ImGui::GetCursorPosY());
}

void Render::top_section_child()
{
    if (!show_window)
        return;

    static auto last_refresh_time = std::chrono::steady_clock::now();
    static bool server_started = false;

    if (!server_started)
    {
        const auto server_exe_path = (Globals::AddonPath / "GW2TP_Python.exe").string();
        if (std::filesystem::exists(server_exe_path))
        {
            RenderUI::start_executable(server_exe_path, "server.exe", false);
            server_started = true;
            (void)Globals::APIDefs->Log(ELogLevel_INFO, "GW2TP", "Auto-started localhost server on first render");
        }
    }

    const auto window_width = ImGui::GetWindowContentRegionWidth();
    ImGui::BeginChild("TopSection", ImVec2(window_width, 160.0f), false, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

    RenderUI::render_top_controls(data, last_refresh_time);
    ImGui::Spacing();
    ImGui::Separator();
    RenderUI::render_profit_calculator();
    ImGui::Separator();
    ImGui::Spacing();

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
    if (!show_window)
        return;

    if (ImGui::Begin("GW2TP", &show_window))
    {
        top_section_child();
        table_child();
    }

    ImGui::End();
}
