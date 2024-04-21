#pragma once

#include <string>
#include <memory>

namespace Broadcast
{
class SuccessHandler
{
public:
    virtual ~SuccessHandler() = default;
    virtual void onConnectSuccess(const std::string& ip) = 0;
};
using SuccessHandlerPtr = std::shared_ptr<SuccessHandler>;
} // namespace Broadcast
