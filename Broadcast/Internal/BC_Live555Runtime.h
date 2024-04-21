#pragma once

#include <memory>
#include <thread>
#include <unordered_map>

class TaskScheduler;
class UsageEnvironment;

namespace Broadcast
{

class ListenerImpl;

class Live555Runtime final
{
public:
    static Live555Runtime& getInstance()
    {
        static Live555Runtime runtime;
        return runtime;
    }

    void initialize(std::shared_ptr<ListenerImpl> listener);

private:
    Live555Runtime();
    ~Live555Runtime();

    static void initListener(void* listener);

private:
    std::thread runnerThread_;

    char runFlag = 0;
    TaskScheduler* scheduler_ = nullptr;
    UsageEnvironment* envir_ = nullptr;

    std::uint32_t openStreamURLEventID_ = 0;
    std::unordered_map<ListenerImpl*, std::shared_ptr<ListenerImpl>> pendingListeners_;
};

} // namespace Broadcast
