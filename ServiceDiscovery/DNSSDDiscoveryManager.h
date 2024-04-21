#pragma once

#include "DNSSDDefs.h"

#include <functional>
#include <map>

class mDNSAsyncRunner;

namespace DNSServiceDiscovery
{
class DiscoveryManager final
{
public:
    static DiscoveryManager& getInstance()
    {
        static DiscoveryManager instance;
        return instance;
    }

    template <typename... Args>
    using Callback = std::function<void(Args...)>;

    using RegistrationCallback = Callback<StatusCode, RegisteredServiceData>;
    void registerService(const RegistrationRequest&, RegistrationCallback);

    using DetectionCallback = Callback<StatusCode, DetectedServiceData>;
    void detectService(const DetectionRequest&, DetectionCallback);

    using ResolveCallback = Callback<StatusCode, ResolvedServiceData>;
    void resolveService(const DetectedServiceData&, ResolveCallback);
    
    using QueryCallback = Callback<StatusCode, std::size_t, std::string>;
    void queryServiceData(const ResolvedServiceData&, ServiceQueryType, QueryCallback);

private:
    DiscoveryManager();
    ~DiscoveryManager();
    DiscoveryManager(const DiscoveryManager&) = delete;
    DiscoveryManager(DiscoveryManager&&) = delete;
    DiscoveryManager& operator=(const DiscoveryManager&) = delete;
    DiscoveryManager& operator=(DiscoveryManager&&) = delete;

private:
    std::unique_ptr<mDNSAsyncRunner> runner_;

    class RecordsHolder;
    std::unique_ptr<RecordsHolder> recordsHolder_;

    class ResponseProcessor;
};

}  // namespace DNSServiceDiscovery
