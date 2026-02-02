#pragma once

#include <future>
#include <list>
#include <map>
#include <string>

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
    bool use_localhost = false;

    std::list<Request> futures;
    std::map<std::string, std::map<std::string, int>> api_data;

    void requesting();
    void storing();
};
