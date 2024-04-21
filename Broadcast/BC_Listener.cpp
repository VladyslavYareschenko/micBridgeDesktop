#include "BC_Listener.h"

#include "Internal/BC_ListenerImpl.h"
#include "Internal/BC_Live555Runtime.h"

namespace Broadcast
{

Listener::Listener(const std::string& ip,
                   std::uint16_t port,
                   const std::string& authCode,
                   AudioFramesHandlerPtr framesHandler,
                   DispatchQueuePtr dispatchQueue,
                   SuccessHandlerPtr successHandler,
                   ErrorHandlerPtr errorHandler)
{
    impl_ = std::make_shared<ListenerImpl>(ip,
                                           port,
                                           authCode,
                                           std::move(framesHandler),
                                           std::move(dispatchQueue),
                                           std::move(successHandler),
                                           std::move(errorHandler));

    Live555Runtime::getInstance().initialize(impl_);
}

Listener::~Listener()
{
    impl_->setExpired(true);
}

} // namespace Broadcast
