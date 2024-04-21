#pragma once

#ifndef __APPLE__
typedef struct mDNS_PlatformSupport_struct mDNS_PlatformSupport;
typedef union CacheEntity_union CacheEntity;
#endif  // __APPLE__

#include <chrono>

class mDNSPlatformIntegration
{
public:
    mDNSPlatformIntegration();
    ~mDNSPlatformIntegration();

    std::chrono::milliseconds poll();

private:
#ifndef __APPLE__
    mDNS_PlatformSupport* mDNSPlatform_;
    CacheEntity* cache_;
#endif  // !__APPLE__
};
