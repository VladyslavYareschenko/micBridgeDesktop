#pragma once

#include <functional>

typedef struct _DNSServiceRef_t* DNSServiceRef;

class mDNSAsyncRunner
{
public:
    mDNSAsyncRunner();
    ~mDNSAsyncRunner();

    using mDNSServiceTask = std::function<DNSServiceRef()>;
    void post(mDNSServiceTask serviceTask);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
