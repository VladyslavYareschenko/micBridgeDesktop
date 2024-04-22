#include "mDNSPlatformIntegration.h"

#ifndef __APPLE__
#if defined(interface)
#undef interface
#endif
#include <mDNSEmbeddedAPI.h>
#if _WIN32
#include <Poll.h>
#include <mDNSWin32.h>
#elif __linux__
#endif

static void mDNSInit_ReportStatus(int, const char*, ...)
{}

extern "C" mDNS mDNSStorage;
extern "C" mDNSs32 mDNSPlatformOneSecond;
#endif  // !__APPLE__

mDNSPlatformIntegration::mDNSPlatformIntegration()
{
#ifndef __APPLE__
    constexpr size_t CacheSize = 256;
    cache_ = new CacheEntity[CacheSize];
    mDNSPlatform_ = new mDNS_PlatformSupport{};
    mDNS_Init(&mDNSStorage,
              mDNSPlatform_,
              cache_,
              CacheSize,
              mDNS_Init_AdvertiseLocalAddresses,
              mDNS_Init_NoInitCallback,
              mDNS_Init_NoInitCallbackContext);

    mDNSPlatform_->reportStatusFunc = mDNSInit_ReportStatus;

    SetupInterfaceList(&mDNSStorage);
    uDNS_SetupDNSConfig(&mDNSStorage);
#endif  // !__APPLE__
}

mDNSPlatformIntegration::~mDNSPlatformIntegration()
{
#ifndef __APPLE__
    mDNS_Close(&mDNSStorage);
    delete mDNSPlatform_;
    delete[] cache_;
#endif  // !__APPLE__
}

std::chrono::milliseconds mDNSPlatformIntegration::poll()
{
#ifdef __APPLE__
    constexpr size_t nextTimerEvent{300};
#else
    // The value returned from mDNS_Execute() is the next time(
    // in absolute platform time units) at which
    // mDNS_Execute() MUST be called again to perform its next necessary
    // operation(e.g.transmitting its next scheduled query packet, etc.)
    auto nextTimerEvent = mDNS_Execute(&mDNSStorage) - mDNS_TimeNow(&mDNSStorage);

    if (nextTimerEvent < 0)
    {
        nextTimerEvent = 0;
    }
    else if (nextTimerEvent > (0x7FFFFFFF / 1000))
    {
        nextTimerEvent = 0x7FFFFFFF / mDNSPlatformOneSecond;
    }
    else
    {
        nextTimerEvent = (nextTimerEvent * 1000) / mDNSPlatformOneSecond;
    }

#if _WIN32
    // Why do we need to poll manually?
    // Without this hack, browsing and querying wouldn't work .
    // This logic is based on similar example in the mDNS repository in mDNSWindows/SystemService/Service.c.
    // Probably need to research opensource solutions built on mDNS (Chromium, OpenHome).
    nextTimerEvent = min(nextTimerEvent, 5);
    mDNSPoll(nextTimerEvent);  // move to mDNS_Execute
#endif  // _WIN32

#endif  // __APPLE__

    return std::chrono::milliseconds{nextTimerEvent};
}
