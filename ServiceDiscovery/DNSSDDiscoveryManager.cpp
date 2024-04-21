#include "DNSSDDiscoveryManager.h"

#include "mDNSAsyncRunner.h"

#include <boost/assert.hpp>
#include <boost/format.hpp>
#include <boost/container_hash/hash.hpp>
#include <boost/range/join.hpp>

#include <iostream>
#include <set>
#include <stdexcept>

#if _WIN32
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#endif  // _WIN32

namespace DNSServiceDiscovery
{
class DiscoveryManager::RecordsHolder final
{
public:
    ~RecordsHolder()
    {
        for (auto ref : boost::join(_registered, _browsed))
        {
            DNSServiceRefDeallocate(ref);
        }
    }

    void addRegisteredRecord(DNSServiceRef ref) { _browsed.insert(ref); }

    void addBrowsedRecord(DNSServiceRef ref) { _browsed.insert(ref); }

    bool lookupBrowsedRecord(DNSServiceRef ref) { return _browsed.find(ref) != _browsed.end(); }

    void addResolvedRecord(const std::string& fullname, const ResolvedServiceData& data)
    {
        _resolved.insert({fullname, data});
    }

    ResolvedServiceData lookupResolvedRecord(const std::string& fullname)
    {
        if (auto iter = _resolved.find(fullname); iter != _resolved.end())
        {
            return iter->second;
        }

        return ResolvedServiceData{{}, {}, {}, {}, {}};
    }

private:
    std::set<DNSServiceRef> _registered;
    std::set<DNSServiceRef> _browsed;

    std::map<std::string, ResolvedServiceData> _resolved;
};

static inline std::unordered_map<std::size_t, DetectedServiceData> detectedServicesRegistry;
static inline std::unordered_map<DNSServiceRef, std::size_t> detectedServicesRefs;

static inline std::unordered_map<std::size_t, ResolvedServiceData> resolvedServicesRegistry;


bool operator==(const DetectedServiceData& left, const DetectedServiceData& right)
{
    return left.name == right.name && left.type == right.type && left.domain == right.domain;
}

static std::vector<DetectedServiceData> detectedServices;

class DiscoveryManager::ResponseProcessor final
{
public:
    static inline std::map<DNSServiceRef, RegistrationCallback> registrationCallbacks;
    static inline std::map<DNSServiceRef, DetectionCallback> detectionCallbacks;
    static inline std::map<DNSServiceRef, ResolveCallback> resolveCallbacks;
    static inline std::map<DNSServiceRef, QueryCallback> queryCallbacks;

    static void handleRegister(DNSServiceRef sdRef,
                               DNSServiceFlags flags,
                               DNSServiceErrorType errorCode,
                               const char* name,
                               const char* regtype,
                               const char* domain,
                               void* context)
    {
        if (errorCode == kDNSServiceErr_NoError)
        {
            DiscoveryManager::getInstance().recordsHolder_->addRegisteredRecord(sdRef);
        }
        else
        {
            DNSServiceRefDeallocate(sdRef);
        }

        if (auto iter = registrationCallbacks.find(sdRef); iter!= registrationCallbacks.end())
        {
            iter->second(errorCode == kDNSServiceErr_NoError ? StatusCode::OK : StatusCode::Error,
                         RegisteredServiceData{name, regtype, domain});
            registrationCallbacks.erase(iter);
        }
    }

    static void handleBrowsed(DNSServiceRef sdRef,
                              DNSServiceFlags flags,
             [[maybe_unused]] uint32_t interfaceIndex,
                              DNSServiceErrorType errorCode,
                              const char* serviceName,
                              const char* regtype,
                              const char* replyDomain,
             [[maybe_unused]] void* context)
    {
        std::cout << "handleBrowsed " << sdRef << " " << ((flags & kDNSServiceFlagsAdd) != 0) << std::endl << std::flush;

        DiscoveryManager::getInstance().recordsHolder_->addBrowsedRecord(sdRef);

        if (auto iter = detectionCallbacks.find(sdRef); iter!= detectionCallbacks.end())
        {
            const auto action = (flags & kDNSServiceFlagsAdd) ? ServiceAction::Add : ServiceAction::Remove;

            std::string name = serviceName;
            std::string type = regtype;
            std::string domain = replyDomain;

            auto id = std::hash<std::string>{}(name);
            boost::hash_combine(id, type);
            boost::hash_combine(id, domain);

            DetectedServiceData data{id, action, std::move(name), std::move(type), std::move(domain)};

            if (action == ServiceAction::Add)
            {
                detectedServicesRegistry.insert({data.id, data});
                detectedServicesRefs.insert({sdRef, data.id});
            }
            else
            {
                detectedServicesRegistry.erase(data.id);
                detectedServicesRefs.erase(sdRef);
            }

            const auto status = errorCode == kDNSServiceErr_NoError ? StatusCode::OK : StatusCode::Error;
            iter->second(status, std::move(data));
        }
    }

    static void handleResolved(DNSServiceRef sdRef,
                               DNSServiceFlags flags,
                               uint32_t interfaceIndex,
                               DNSServiceErrorType errorCode,
                               const char* hosttarget,
                               const char* fullname,
                               uint16_t port, /* In network byte order */
                               uint16_t txtLen,
                               const unsigned char* txtRecord,
                               void* context)
    {
        std::cout << "handleResolved " << sdRef << " " << ((flags & kDNSServiceFlagsAdd) != 0) << std::endl << std::flush;

        if (detectedServicesRefs.find(sdRef) == detectedServicesRefs.end())
        {
            DNSServiceRefDeallocate(sdRef);
        }

        std::vector<DNSServiceDiscovery::TXTRecord> records;
        {
            constexpr std::size_t KeyBufQuota = 256;
            char keyBuf[KeyBufQuota];
            const size_t size = TXTRecordGetCount(txtLen, txtRecord);
            for (auto i = 0u; i < size; ++i)
            {
                std::memset(keyBuf, 0, KeyBufQuota);

                std::uint8_t valLen = 0;
                const void* val = nullptr;

                const auto status =
                    TXTRecordGetItemAtIndex(txtLen, txtRecord, i, KeyBufQuota, keyBuf, &valLen, &val);

                if (status == kDNSServiceErr_NoError)
                {
                    records.push_back({keyBuf,
                                       std::string{static_cast<const char*>(val),
                                                   static_cast<const char*>(val) + valLen}});
                }
                else
                {
                    std::cerr << boost::format("DNSServiceDiscovery: Cannot parse "
                                               "TXT records buffer. Code - %1%")
                                     % status
                              << std::endl
                              << std::flush;
                }
            }
        }

        auto* detectedData = context ? reinterpret_cast<DetectedServiceData*>(context) : nullptr;
        auto id = detectedData ? detectedData->id : 0;

        if (id)
        {
            auto name = detectedData ? detectedData->name : std::string{};
            ResolvedServiceData serviceData{id,
                                            std::move(name),
                                            std::string(fullname),
                                            std::string(hosttarget),
                                            std::uint16_t(port),
                                            std::uint32_t(interfaceIndex),
                                            std::move(records)};

            resolvedServicesRegistry.insert({id, serviceData});

            if (auto iter = resolveCallbacks.find(sdRef); iter!= resolveCallbacks.end())
            {
                const auto status = errorCode == kDNSServiceErr_NoError ? StatusCode::OK : StatusCode::Error;
                iter->second(status, std::move(serviceData));
                resolveCallbacks.erase(iter);
            }
        }
    }

    static void handleQueryResult(DNSServiceRef sdRef,
                                  DNSServiceFlags flags,
                 [[maybe_unused]] uint32_t interfaceIndex,
                                  DNSServiceErrorType errorCode,
                 [[maybe_unused]] const char* fullname,
                 [[maybe_unused]] uint16_t rrtype,
                                  uint16_t rrclass,
                 [[maybe_unused]] uint16_t rdlen,
                                  const void* rdata,
                 [[maybe_unused]] uint32_t ttl,
                                  void* context)
    {
        auto result = [rrclass, rdata]() -> std::string
        {
            switch (rrclass)
            {
            case kDNSServiceType_A:
            {
                char ip_char[INET_ADDRSTRLEN];
                if (inet_ntop(AF_INET, rdata, ip_char, INET_ADDRSTRLEN))
                {
                    return {ip_char};
                }

                return {};
            }
            default:
                BOOST_ASSERT_MSG(false, "Unsupported Service Query reply type in DNSServiceDiscovery::DiscoveryManager.");

            return {};
            };
        }();

        std::cout << "handleQueryResult " << sdRef << " " << (flags & kDNSServiceFlagsAdd) << std::endl << std::flush;

        if (detectedServicesRefs.find(sdRef) == detectedServicesRefs.end())
        {
            DNSServiceRefDeallocate(sdRef);
        }

        auto id = context ? reinterpret_cast<ResolvedServiceData*>(context)->id : 0;

        if (auto iter = queryCallbacks.find(sdRef); iter!= queryCallbacks.end())
        {
            if (id)
            {
                const auto status = errorCode == kDNSServiceErr_NoError ? StatusCode::OK : StatusCode::Error;
                iter->second(status, id, std::move(result));
            }

            queryCallbacks.erase(iter);
        }
    }
};

DiscoveryManager::DiscoveryManager()
    : runner_(std::make_unique<mDNSAsyncRunner>())
    , recordsHolder_(std::make_unique<RecordsHolder>())
{}

DiscoveryManager::~DiscoveryManager() = default;

namespace
{
template <typename T>
std::string buildRegType(const T& request)
{
    return (boost::format("_%1%._%2%") % request.serviceName %
            [prot = request.protocol]
            {
                switch (prot)
                {
                case TransportProtocol::TCP:
                    return "tcp";
                case TransportProtocol::UDP:
                    return "udp";
                }
            }())
        .str();
}

template <typename T>
constexpr void* makeVoidPtr(T* ptr)
{
    return reinterpret_cast<void*>(ptr);
}
}  // namespace

void DiscoveryManager::registerService(const RegistrationRequest& request,
                                       RegistrationCallback callback)
{
    runner_->post(
        [type = buildRegType(request),
         port = request.destinationPort,
         requestRecords = request.txtRecords,
         reply = std::move(callback)
        ]() -> DNSServiceRef
        {
            constexpr std::size_t TXTRecordQuota = 256;
            char txtRecordBuffer[TXTRecordQuota];

            TXTRecordRef records;
            TXTRecordCreate(&records, TXTRecordQuota, txtRecordBuffer);

            std::size_t recordsLen = 0;
            for (const auto& record : requestRecords)
            {
                recordsLen += record.key.length() + 1 + record.value.length() + 1;
                if (recordsLen > TXTRecordQuota)
                {
                    TXTRecordDeallocate(&records);
                    throw std::runtime_error(
                        "DiscoveryManager::registerService TXT "
                        "Records quota exceeded.");
                }

                TXTRecordSetValue(&records,
                                  record.key.c_str(),
                                  record.value.length(),
                                  record.value.c_str());
            }

            constexpr DNSServiceFlags NoneFlags = 0;
            constexpr auto IFaceIndexAny = kDNSServiceInterfaceIndexAny;
            constexpr const char* NullName = nullptr;
            constexpr const char* NullDomain = nullptr;
            constexpr const char* NullHostName = nullptr;
            DNSServiceRef service = nullptr;
            const auto registerStatus =
                DNSServiceRegister(&service,
                                   NoneFlags,
                                   IFaceIndexAny,
                                   NullName,
                                   type.c_str(),
                                   NullDomain,
                                   NullHostName,
                                   port,
                                   TXTRecordGetLength(&records),
                                   TXTRecordGetBytesPtr(&records),
                                   ResponseProcessor::handleRegister,
                                   nullptr);

            TXTRecordDeallocate(&records);

            if (registerStatus != kDNSServiceErr_NoError)
            {
                reply(StatusCode::Error, RegisteredServiceData{{}, {}, {}});

                return nullptr;
            }

            ResponseProcessor::registrationCallbacks.insert({service, std::move(reply)});

            return service;
        });
}

void DiscoveryManager::detectService(const DetectionRequest& request,
                                     DetectionCallback callback)
{
    runner_->post(
        [type = buildRegType(request), reply = std::move(callback)]() -> DNSServiceRef
        {
            constexpr DNSServiceFlags NoneFlags = 0;
            constexpr auto IFaceIndexAny = kDNSServiceInterfaceIndexAny;
            constexpr const char* NullDomain = nullptr;

            DNSServiceRef service = nullptr;
            const auto browseStatus = DNSServiceBrowse(&service,
                                                       NoneFlags,
                                                       IFaceIndexAny,
                                                       type.c_str(),
                                                       NullDomain,
                                                       ResponseProcessor::handleBrowsed,
                                                       nullptr);

            if (browseStatus != kDNSServiceErr_NoError)
            {
                reply(StatusCode::Error, DetectedServiceData{{}, {}, {}, {}, {}});

                return nullptr;
            }

            ResponseProcessor::detectionCallbacks.insert({service, std::move(reply)});

            return service;
        });
}

void DiscoveryManager::resolveService(const DetectedServiceData& request,
                                      ResolveCallback callback)
{
//    if (!DiscoveryManager::getInstance().recordsHolder_->lookupResolvedRecord(request.name).fullname.empty())
//    {
//        callback(StatusCode::OK, DiscoveryManager::getInstance().recordsHolder_->lookupResolvedRecord(request.name));

//        return;
//    }

    runner_->post(
        [request, reply = std::move(callback)]() -> DNSServiceRef
        {
            constexpr DNSServiceFlags NoneFlags = 0;
            constexpr auto IFaceIndexAny = kDNSServiceInterfaceIndexAny;

            auto detectedService = detectedServicesRegistry.find(request.id);
            // Iterator invalidation does not affect referenced elements, so don't worry.
            auto* userData = detectedService != detectedServicesRegistry.end() ? &(detectedService->second) : nullptr;

            DNSServiceRef service = nullptr;
            const auto resolveStatus = DNSServiceResolve(&service,
                                                         NoneFlags,
                                                         IFaceIndexAny,
                                                         request.name.c_str(),
                                                         request.type.c_str(),
                                                         request.domain.c_str(),
                                                         ResponseProcessor::handleResolved,
                                                         userData);


            if (resolveStatus != kDNSServiceErr_NoError)
            {
                reply(StatusCode::Error, ResolvedServiceData{{}, {}, {}, {}, {}, {}});

                return nullptr;
            }

            ResponseProcessor::resolveCallbacks.insert({service, std::move(reply)});

            return service;
        });
}


void DiscoveryManager::queryServiceData(const ResolvedServiceData& request,
                                        const ServiceQueryType queryType,
                                        QueryCallback callback)
{
    if (!request.id)
    {
        return;
    }

    const auto mDNSQueryType = [queryType]
    {
        switch (queryType)
        {
        case ServiceQueryType::IPv4:
            return kDNSServiceType_A;
        };
    }();

    runner_->post(
        [mDNSQueryType, request, reply = std::move(callback)]() -> DNSServiceRef
        {
            constexpr DNSServiceFlags NoneFlags = 0;

            auto resolvedService = resolvedServicesRegistry.find(request.id);
            // Iterator invalidation does not affect referenced elements, so don't worry.
            auto* userData = resolvedService != resolvedServicesRegistry.end() ? &(resolvedService->second) : nullptr;

            DNSServiceRef service = nullptr;
            const auto resolveStatus =
                DNSServiceQueryRecord(&service,
                                      NoneFlags,
                                      request.interfaceIndex,
                                      request.fullname.c_str(),
                                      mDNSQueryType,
                                      kDNSServiceClass_IN,
                                      ResponseProcessor::handleQueryResult,
                                      userData);

            if (resolveStatus != kDNSServiceErr_NoError)
            {
                reply(StatusCode::Error, 0, std::string{});

                return nullptr;
            }

            ResponseProcessor::queryCallbacks.insert({service, std::move(reply)});

            return service;
        });
}

}  // namespace DNSServiceDiscovery
