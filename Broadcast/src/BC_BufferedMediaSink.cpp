#include "BC_BufferedMediaSink.h"

#include <uLawAudioFilter.hh>

#include <chrono>
#include <ctime>
#include <iostream>

namespace Broadcast
{

BufferedMediaSink* BufferedMediaSink::createNew(UsageEnvironment& env,
                                                MediaSubsession& subsession,
                                                char const* streamId)
{
  return new BufferedMediaSink(env, subsession, streamId);
}

BufferedMediaSink::BufferedMediaSink(UsageEnvironment& env, MediaSubsession&, char const* streamID)
    : MediaSink(env)
    , streamID_(streamID)
{
}

void BufferedMediaSink::setFramesHandler(std::weak_ptr<AudioFramesHandler> handler)
{
  framesHandler_ = handler;
}

void BufferedMediaSink::afterGettingFrame(void* clientData,
                                          std::uint32_t frameSize,
                                          std::uint32_t numTruncatedBytes,
                                          struct timeval presentationTime,
                                          std::uint32_t durationInMicroseconds)
{
  auto* self = reinterpret_cast<BufferedMediaSink*>(clientData);
  self->afterGettingFrame(frameSize, numTruncatedBytes, presentationTime, durationInMicroseconds);
}

namespace
{
void normalizeFrame(std::uint8_t* data, std::size_t length)
{
  unsigned numValues = length / 2;
  u_int16_t* value = (u_int16_t*)data;
  for (unsigned i = 0; i < numValues; ++i) {
    u_int16_t const orig = value[i];
    value[i] = ((orig & 0xFF) << 8) | ((orig & 0xFF00) >> 8);
  }
}
} // namespace

void BufferedMediaSink::afterGettingFrame(unsigned frameSize,
                                          unsigned numTruncatedBytes,
                                          struct timeval /*presentationTime*/,
                                          unsigned /*durationInMicroseconds*/)
{
    if (isExprired_)
        return;

    if (numTruncatedBytes != 0)
        std::cout << numTruncatedBytes << std::endl << std::flush;

  // Normalize audio frame after transmission
//  normalizeFrame(recieveBuffer_.data(), frameSize);

  // Notify client
  if (auto handler = framesHandler_.lock())
  {
    handler->onFrame(recieveBuffer_.data(), frameSize);
  }

  // Then continue, to request the next frame of data:
  continuePlaying();
}

Boolean BufferedMediaSink::continuePlaying()
{
  if (fSource == NULL)
    return False; // sanity check (should not happen)

  if (!swapEndianFilter_)
  {
      swapEndianFilter_ = EndianSwap16::createNew(this->envir(), fSource);

      if (swapEndianFilter_ == NULL)
      {
          envir() << "Unable to create a little->bit-endian order filter from the "
                     "PCM audio source: "
                  << envir().getResultMsg() << "\n";
          return False;
      }
  }

  // Request the next frame of data from our input source.
  // "afterGettingFrame()" will get called later, when it arrives:
  fSource->getNextFrame(recieveBuffer_.data(),
                        recieveBuffer_.size(),
                        afterGettingFrame,
                        this,
                        onSourceClosure,
                        this);

  return True;
}

} // namespace Broadcast
