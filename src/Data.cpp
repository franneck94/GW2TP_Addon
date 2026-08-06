#include <iostream>
#include <list>
#include <map>
#include <string>

#include "nlohmann/json.hpp"

#include "httpclient/httpclient.h"

#include "API.h"
#include "Constants.h"
#include "Data.h"
#include "Settings.h"

using json = nlohmann::json;

static OrderedIntValues _collect_json(const json &jval, const std::string &prefix)
{
    OrderedIntValues kv;

    if (jval.is_object())
    {
        for (auto it = jval.begin(); it != jval.end(); ++it)
        {
            auto _kv = _collect_json(it.value(), prefix.empty() ? it.key() : prefix + "." + it.key());
            kv.insert(kv.end(), _kv.begin(), _kv.end());
        }
    }
    else if (jval.is_array())
    {
        for (size_t i = 0; i < jval.size(); ++i)
        {
            auto _kv = _collect_json(jval[i], prefix + "[" + std::to_string(i) + "]");
            kv.insert(kv.end(), _kv.begin(), _kv.end());
        }
    }
    else
    {
        if (jval.is_number_integer())
        {
            kv.emplace_back(prefix, static_cast<int>(jval));
        }
    }

    return kv;
}

static OrderedStringValues _collect_json_strings(const json &jval, const std::string &prefix)
{
    OrderedStringValues kv;

    if (jval.is_object())
    {
        for (auto it = jval.begin(); it != jval.end(); ++it)
        {
            auto _kv = _collect_json_strings(it.value(), prefix.empty() ? it.key() : prefix + "." + it.key());
            kv.insert(kv.end(), _kv.begin(), _kv.end());
        }
    }
    else if (jval.is_array())
    {
        for (size_t i = 0; i < jval.size(); ++i)
        {
            auto _kv = _collect_json_strings(jval[i], prefix + "[" + std::to_string(i) + "]");
            kv.insert(kv.end(), _kv.begin(), _kv.end());
        }
    }
    else if (jval.is_string())
    {
        kv.emplace_back(prefix, jval.get<std::string>());
    }

    return kv;
}

void Data::requesting()
{
    if (!requested)
    {
        std::wcout << "Requesting data from API...\n";
        futures.clear();

        const auto &base_url = API::LOCAL_API_URL;

        for (auto command : API::COMMANDS)
        {
            if (command == "ecto")
                command = "price?item_id=19721";
            else if (command == "rare_gear")
                command = "price?item_id=83008";
            else if (command == "krait_shield_craft" && Settings::EctoRate != 0.90F)
                command = "krait_shield_craft?ecto_rate=" + std::to_string(Settings::EctoRate);
            else if (command == "krait_trident_craft" && Settings::EctoRate != 0.90F)
                command = "krait_trident_craft?ecto_rate=" + std::to_string(Settings::EctoRate);

            const auto wstr_url = base_url + L"/" + std::wstring(command.begin(), command.end());
            auto future = HTTPClient::GetRequestAsync(wstr_url);
            auto req = Request(std::move(command), std::move(future));
            futures.push_back(std::move(req));
        }

        requested = true;
        api_data.clear();
        api_string_data.clear();
    }
}

void Data::storing()
{
    if (futures.size() == 0)
        loaded = true;
    else
        loaded = false;

    auto it = futures.begin();

    while (it != futures.end())
    {
        if (it->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        {
            auto j = json{};
            auto request_id = it->request_id;
            std::string request;
            try
            {
                request = it->future.get();
                j = json::parse(request);
            }
            catch (const std::exception &e)
            {
                std::cerr << "Request failed for request_id '" << request_id << "': " << e.what() << std::endl;
                it = futures.erase(it);
                return;
            }

            if (request_id == "price?item_id=19721")
                request_id = "ecto";
            else if (request_id == "price?item_id=83008")
                request_id = "rare_gear";
            else if (request_id.find("krait_shield_craft") != std::string::npos)
                request_id = "krait_shield_craft";
            else if (request_id.find("krait_trident_craft") != std::string::npos)
                request_id = "krait_trident_craft";

            auto kv = _collect_json(j, "");
            auto string_kv = _collect_json_strings(j, "");

            api_data[request_id] = kv;
            api_string_data[request_id] = string_kv;
            it = futures.erase(it);
            return; /* return early in this frame */
        }

        ++it;
    }
}
