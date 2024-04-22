#include "mDNSAsyncRunner.h"

#include "mDNSPlatformIntegration.h"

#include <boost/asio/io_context.hpp>
#include <boost/asio/post.hpp>
#include <boost/asio/steady_timer.hpp>

#include <dns_sd.h>

#include <iostream>
#include <thread>
#include <queue>

class mDNSAsyncRunner::Impl
{
public:
    Impl()
        : keepAliveTimer_(context_)
    {
        launch();
    }

    virtual ~Impl()
    {
        keepAliveTimer_.cancel();
        interrupted_ = true;
        //context_.stop();
        thread_.join();
    }

    void post(mDNSServiceTask serviceTask)
    {
        boost::asio::post(context_,
                          [this, serviceTask = std::move(serviceTask)]
                          {
                              if (auto* result = serviceTask())
                              {
                                  DNSServiceProcessResult(result);
                                  poll();
                              }
                          });
    }

    void launch()
    {
        auto entry = [this]()
        {
            try
            {
                poll();
                context_.run();
            }
            catch (const std::exception& e)
            {
                std::cerr << e.what() << std::endl << std::flush;
            }
        };

        thread_ = std::thread(std::move(entry));
        //thread_.detach();
    }


    void poll()
    {
        if (interrupted_)
        {
            keepAliveTimer_.cancel();
            return;
        }

        const auto nextEventTime = platformIntegration_.poll();
        keepAliveTimer_.expires_after(nextEventTime);
        keepAliveTimer_.async_wait(
            [this](const auto& status)
            {
                // std::cout << " poll()" << std::endl << std::flush;

//                if (status == boost::asio::error::operation_aborted)
//                {
//                     std::cout << "boost::asio::error::operation_aborted" << std::endl << std::flush;
//                     return;
//                }

                if (interrupted_)
                {
                    keepAliveTimer_.cancel();
                    return;
                }

                poll();
            });
    }

private:
    mDNSPlatformIntegration platformIntegration_;

    boost::asio::io_context context_;
    boost::asio::steady_timer keepAliveTimer_;

    std::atomic_bool interrupted_ = false;
    std::thread thread_;
};

mDNSAsyncRunner::mDNSAsyncRunner()
    : impl_(std::make_unique<Impl>())
{
}

mDNSAsyncRunner::~mDNSAsyncRunner() = default;

void mDNSAsyncRunner::post(mDNSServiceTask serviceTask)
{
    impl_->post(std::move(serviceTask));
}
