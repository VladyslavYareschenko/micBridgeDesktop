#pragma once

#include <string>
#include <vector>

namespace DNSServiceDiscovery
{
enum class TransportProtocol : std::uint8_t
{
    TCP = 0,
    UDP
};

enum class StatusCode : std::uint8_t
{
    OK = 0,
    Error
};

enum class ServiceAction : std::uint8_t
{
    Add = 0,
    Remove
};

struct TXTRecord
{
    std::string key;
    std::string value;
};

struct RegistrationRequest
{
    std::string serviceName;
    std::vector<TXTRecord> txtRecords;

    TransportProtocol protocol;
    std::uint16_t destinationPort;
};

inline std::vector<TXTRecord> TXTRecordsEmpty()
{
    return {};
}

struct RegisteredServiceData
{
    const std::string name;
    const std::string type;
    const std::string domain;
};

struct DetectionRequest
{
    std::string serviceName;
    TransportProtocol protocol;
};

struct DetectedServiceData
{
    std::size_t id;
    ServiceAction action;
    std::string name;
    std::string type;
    std::string domain;
};

struct ResolvedServiceData
{
    std::size_t id;
    std::string name;
    std::string fullname;
    std::string hosttarget;
    std::uint16_t port;
    std::uint32_t interfaceIndex;
    std::vector<TXTRecord> txtRecords;
};

enum class ServiceQueryType
{
    IPv4 = 0
};

}  // namespace DNSServiceDiscovery
