add_library(ServiceDiscovery_interface INTERFACE)

add_library(ServiceDiscovery::interface ALIAS ServiceDiscovery_interface)

target_include_directories(ServiceDiscovery_interface
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

add_library(ServiceDiscovery
  src/DNSSDDiscoveryManager.cpp
  src/mDNSAsyncRunner.cpp
  src/mDNSPlatformIntegration.cpp
)

add_subdirectory(mDNSResponder EXCLUDE_FROM_ALL)

target_link_libraries(ServiceDiscovery 
	PUBLIC ServiceDiscovery::interface
        PRIVATE Boost::asio Boost::assert Boost::format Boost::range mDNSResponder)

target_include_directories(ServiceDiscovery
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${Boost_INCLUDE_DIRS})
