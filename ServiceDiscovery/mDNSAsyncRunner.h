#pragma once

#include <dns_sd.h>

#include <functional>

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