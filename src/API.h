
#pragma once

#include <array>
#include <set>
#include <string>

class API
{
public:
    static const inline std::wstring PRODUCTION_API_URL = L"https://gw2tp-production.up.railway.app/api";

    static const inline std::set<std::string> COMMANDS_LIST = {
        // runes
        "scholar_rune",
        "dragonhunter_rune",
        "guardian_rune",
        // relics
        "relic_of_fireworks",
        "relic_of_aristocracy",
        "relic_of_thief",
        // rare / ecto
        "rare_weapon_craft",
        "rare_gear_salvage",
        "ecto",
        "rare_gear",
        // gear
        "gear_salvage",
        "common_gear_salvage",
        // t5
        "t5_mats_buy",
        "mats_crafting_compare",
        // forge
        "symbol_enh_forge",
        "loadstone_forge",
        // other
        "thesis_on_masterful_malice"};

    static const inline std::array<const char *, 4> RARE_GEAR_NAMES = {
        "stack_buy",
        "salvage_costs",
        "mats_value_after_tax",
        "profit_stack",
    };

    static const inline std::array<const char *, 4> GEAR_SALVAGE_NAMES = {
        "stack_buy",
        "salvage_costs",
        "mats_value_after_tax",
        "profit_stack",
    };

    static const inline std::array<const char *, 7> T5_MATS_BUY_NAMES = {
        "large_claw",
        "potent_blood",
        "large_bone",
        "intricate_totem",
        "large_fang",
        "potent_venom",
        "large_scale",
    };

    static const inline std::array<const char *, 6> MATS_CRAFTING_COMPARE_NAMES = {
        "mithril_ore_to_ingot",
        "mithril_ingot_buy",
        "elder_wood_log_to_plank",
        "elder_wood_plank_buy",
        "lucent_mote_to_crystal",
        "lucent_crystal_buy",
    };

    static const inline std::array<const char *, 4> COMMON_GEAR_NAMES = {
        "stack_buy",
        "salvage_costs",
        "mats_value_after_tax",
        "profit_stack",
    };

    static const inline std::array<const char *, 4> LOADSTONE_NAMES = {
        "onyx",
        "charged",
        "corrupted",
        "destroyer",
    };

    static const inline std::array<const char *, 4> THESIS_MASTERFUL_MALICE = {
        "crafting_cost",
        "sell",
        "flip",
        "profit",
    };

    static const inline std::array<const char *, 3> SCHOLAR_RUNE_NAMES = {
        "crafting_cost",
        "sell",
        "profit",
    };

    static const inline std::array<const char *, 3> GUARDIAN_RUNE_NAMES = {
        "crafting_cost",
        "sell",
        "profit",
    };

    static const inline std::array<const char *, 3> DRAGONHUNTER_RUNE_NAMES = {
        "crafting_cost",
        "sell",
        "profit",
    };

    static const inline std::array<const char *, 4> FIREWORKS_NAMES = {
        "crafting_cost",
        "sell",
        "flip",
        "profit",
    };

    static const inline std::array<const char *, 4> THIEF_NAMES = {
        "crafting_cost",
        "sell",
        "flip",
        "profit",
    };

    static const inline std::array<const char *, 4> ARISTOCRACY_NAMES = {
        "crafting_cost",
        "sell",
        "flip",
        "profit",
    };

    static const inline std::array<const char *, 3> RARE_WEAPON_CRAFT_NAMES = {
        "crafting_cost",
        "ecto_sell_after_tax",
        "profit",
    };

    static const inline std::array<const char *, 3> FORGE_ENH_NAMES = {
        "cost",
        "profit_per_try",
        "profit_per_shard",
    };
};
