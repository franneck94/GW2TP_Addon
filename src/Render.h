#pragma once

#include <cstdint>
#include <set>
#include <string>

#include "Data.h"

class Render
{
public:
    constexpr static auto TABLE_HEIGHT_PX = 150.0F;
    constexpr static auto NAME_COLUMN_WIDTH_PX = 140.0F;
    constexpr static auto NUMBER_COLUMN_WIDTH_PX = 25.0F;
    constexpr static auto OFFSET_PX = 40.0F;

    Data data;
    bool &show_window;

    void render();

    Render(bool &show_window) : show_window(show_window) {}

private:
    int render_table(const std::string &request_id);
    void top_section_child();

    void table_child();
    void render_tables_for_commands(const std::set<std::string> &commands, uint32_t &idx);
};
