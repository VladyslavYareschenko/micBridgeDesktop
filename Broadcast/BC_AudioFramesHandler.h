#pragma once

#include <memory>

namespace Broadcast
{
class AudioFramesHandler
{
 public:
  virtual ~AudioFramesHandler() = default;
  virtual void onFrame(const std::uint8_t*, std::size_t len) = 0;
};
using AudioFramesHandlerPtr = std::shared_ptr<AudioFramesHandler>;
} // namespace Broadcast
