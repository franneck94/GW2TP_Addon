#include <chrono>
#include <iostream>
#include <list>
#include <map>
#include <ranges>
#include <string>

#include "httpclient/httpclient.h"
#include "imgui.h"

#include "API.h"
#include "Constants.h"
#include "Data.h"
#include "Render.h"

namespace
{
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

    void add_header(const std::string name)
    {
        const auto transformed_name = get_clean_category_name(name, false);

        ImGui::TableSetupColumn(transformed_name.c_str(), ImGuiTableColumnFlags_WidthFixed, Render::NAME_COLUMN_WIDTH_PX);
        ImGui::TableSetupColumn("G", ImGuiTableColumnFlags_WidthFixed, Render::NUMBER_COLUMN_WIDTH_PX);
        ImGui::TableSetupColumn("S", ImGuiTableColumnFlags_WidthFixed, Render::NUMBER_COLUMN_WIDTH_PX);
        ImGui::TableSetupColumn("C", ImGuiTableColumnFlags_WidthFixed, Render::NUMBER_COLUMN_WIDTH_PX);
        ImGui::TableHeadersRow();

        // Check for hover and double-click after headers are rendered
        if (ImGui::TableGetColumnFlags(0) & ImGuiTableColumnFlags_IsHovered && ImGui::IsMouseDoubleClicked(0))
            ImGui::SetClipboardText(transformed_name.c_str());
        if (ImGui::TableGetColumnFlags(1) & ImGuiTableColumnFlags_IsHovered && ImGui::IsMouseDoubleClicked(0))
            ImGui::SetClipboardText(transformed_name.c_str());
        if (ImGui::TableGetColumnFlags(2) & ImGuiTableColumnFlags_IsHovered && ImGui::IsMouseDoubleClicked(0))
            ImGui::SetClipboardText(transformed_name.c_str());
        if (ImGui::TableGetColumnFlags(3) & ImGuiTableColumnFlags_IsHovered && ImGui::IsMouseDoubleClicked(0))
            ImGui::SetClipboardText(transformed_name.c_str());
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
    if (API::COMMANDS_LIST.find(request_id) == API::COMMANDS_LIST.end() || data.api_data.find(request_id) == data.api_data.end() || data.api_data[request_id].empty())
    {
        ImGui::Text("No data yet for %s", request_id.c_str());

        return -1;
    }

    ImGuiTableFlags flags = ImGuiTableFlags_Borders | ImGuiTableColumnFlags_NoSort;
    if (ImGui::BeginTable(("Prices##" + request_id).c_str(), 4, flags))
    {
        add_header(request_id);

        if (API::COMMANDS_LIST.find(request_id) != API::COMMANDS_LIST.end())
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

    auto *checkbox_label = "Use localhost server";
    center_next_checkbox(checkbox_label);
    if (ImGui::Checkbox(checkbox_label, &data.use_localhost))
    {
        data.loaded = false;
        data.requested = false;
        data.api_data.clear();
        data.futures.clear();
        data.requesting();
        last_refresh_time = std::chrono::steady_clock::now();
        first_load = false;
    }

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
    render_tables_for_commands(API::REGULAR_COMMANDS_LIST, idx);
    render_tables_for_commands(API::RUNE_COMMANDS_LIST, idx);
    render_tables_for_commands(API::SIGIL_COMMANDS_LIST, idx);
    render_tables_for_commands(API::RELIC_COMMANDS_LIST, idx);

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
