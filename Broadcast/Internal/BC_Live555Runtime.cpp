#include "BC_Live555Runtime.h"

#include "BC_ListenerImpl.h"

#include <BasicUsageEnvironment.hh>

namespace Broadcast
{

Live555Runtime::Live555Runtime()
{
    scheduler_ = BasicTaskScheduler::createNew();
    envir_ = BasicUsageEnvironment::createNew(*scheduler_);

    openStreamURLEventID_ = scheduler_->createEventTrigger(Live555Runtime::initListener);

    runnerThread_ = std::thread(
        [this]()
        {
            scheduler_->doEventLoop(&runFlag);
            runFlag = 1;
        });
}

Live555Runtime::~Live555Runtime()
{
    runFlag = 2;
    while(runFlag != 1)  { }
    delete scheduler_;
    runnerThread_.join();
}

void Live555Runtime::initialize(std::shared_ptr<ListenerImpl> listener)
{
    auto iter = pendingListeners_.insert({listener.get(), std::move(listener)});
    scheduler_->triggerEvent(openStreamURLEventID_, iter.first->first);
}

void Live555Runtime::initListener(void* data)
{
    auto* listener = reinterpret_cast<ListenerImpl*>(data);
    if (!listener->isExpired())
    {
        listener->openURL(getInstance().envir_);
    }
    getInstance().pendingListeners_.erase(listener);
}

} // namespace Broadcast
