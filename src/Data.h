#pragma once

#include <future>
#include <list>
#include <map>
#include <string>
#include <utility>
#include <vector>

struct Price
{
    int copper;
    int silver;
    int gold;
};

struct PriceTriplet
{
    Price buy;
    Price sell;
    Price profit;
};

using OrderedIntValues = std::vector<std::pair<std::string, int>>;
using OrderedStringValues = std::vector<std::pair<std::string, std::string>>;

struct Request
{
    std::string request_id;
    std::future<std::string> future;

    Request(Request &&other) noexcept
        : request_id(std::move(other.request_id)),
          future(std::move(other.future))
    {
    }

    Request(std::string &&request_id, std::future<std::string> &&future) noexcept
        : request_id(std::move(request_id)),
          future(std::move(future))
    {
    }

    Request(const Request &) = delete;
    Request() = delete;
};

class Data
{
public:
    bool requested = false;
    bool loaded = false;

    std::list<Request> futures;
    std::map<std::string, OrderedIntValues> api_data;
    std::map<std::string, OrderedStringValues> api_string_data;

    void requesting();
    void storing();
};
