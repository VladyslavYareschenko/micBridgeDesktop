#pragma once

#include <string>
#include <memory>

namespace Broadcast
{
class ErrorHandler
{
 public:
  virtual ~ErrorHandler() = default;
  virtual void onErrorOccured(int code, const std::string& description) = 0;
};
using ErrorHandlerPtr = std::shared_ptr<ErrorHandler>;
} // namespace Broadcast
