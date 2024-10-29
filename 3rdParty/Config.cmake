if (WIN32)
    set (THIRDPARTY_ROOT "$ENV{SYSTEMDRIVE}/micBridge-3rdParty")
else()
    set (THIRDPARTY_ROOT "$ENV{HOME}/micBridge-3rdParty")
endif()

set (THIRDPARTY_DOWNLOAD_ROOT "${THIRDPARTY_ROOT}/download" CACHE INTERNAL "")
set (THIRDPARTY_INSTALL_ROOT "${THIRDPARTY_ROOT}/install" CACHE INTERNAL "")

include(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/Boost.cmake)
include(${CMAKE_CURRENT_SOURCE_DIR}/3rdParty/Qt5.cmake)
