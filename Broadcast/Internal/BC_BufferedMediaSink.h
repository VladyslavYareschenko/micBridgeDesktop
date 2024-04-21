#pragma once

#include "../BC_AudioFramesHandler.h"

#include <MediaSink.hh>
#include <MediaSession.hh>

#include <array>
#include <cstdint>
#include <memory>
#include <string>

namespace Broadcast
{
class BufferedMediaSink : public MediaSink
{
public:
  static BufferedMediaSink* createNew(UsageEnvironment& env, MediaSubsession& subsession, char const* streamID = NULL);

  void setFramesHandler(std::weak_ptr<AudioFramesHandler> framesHandler);

  void setExpired() { isExprired_ = true; }

private:
  BufferedMediaSink(UsageEnvironment& env, MediaSubsession& subsession, char const* streamID);

  virtual ~BufferedMediaSink() = default;

  Boolean continuePlaying() override;

  void afterGettingFrame(std::uint32_t frameSize,
                         std::uint32_t numTruncatedBytes,
                         timeval presentationTime,
                         std::uint32_t durationInMicroseconds);

  static void afterGettingFrame(void* clientData,
                                std::uint32_t frameSize,
                                std::uint32_t numTruncatedBytes,
                                timeval presentationTime,
                                std::uint32_t durationInMicroseconds);

private:
    bool isExprired_ = false;

  std::string streamID_;

  FramedFilter* swapEndianFilter_ = nullptr;

  std::array<u_int8_t, 1024 * 64> recieveBuffer_;

  std::weak_ptr<AudioFramesHandler> framesHandler_;
};
} // namespace Broadcast
