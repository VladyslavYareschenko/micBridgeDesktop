#pragma once

#include "BC_AudioFramesHandler.h"
#include "BC_DispatchQueue.h"
#include "BC_ErrorHandler.h"
#include "BC_SuccessHandler.h"

#include <string>

namespace Broadcast
{

class ListenerImpl;

class Listener final
{
 public:
  Listener(const std::string& ip,
           std::uint16_t port,
           const std::string& authCode,
           AudioFramesHandlerPtr framesHandler,
           DispatchQueuePtr dispatchQueue,
           SuccessHandlerPtr successHandler,
           ErrorHandlerPtr errorHandler);

  ~Listener();

 private:
  std::shared_ptr<ListenerImpl> impl_;
};

} // namespace Broadcast
