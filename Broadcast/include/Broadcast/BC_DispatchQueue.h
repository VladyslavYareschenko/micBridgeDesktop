#pragma once

#include <functional>

namespace Broadcast
{
class DispatchQueue
{
 public:
  virtual ~DispatchQueue() = default;

  using VoidEvent = std::function<void()>;
  virtual void dispatchEvent(VoidEvent) = 0;
};

using DispatchQueuePtr = std::shared_ptr<DispatchQueue>;
} // namespace Broadcast
