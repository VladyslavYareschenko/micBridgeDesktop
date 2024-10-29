message("-- Preparing Boost...")

include(FetchContent)

set(BOOST_ROOT ${THIRDPARTY_INSTALL_ROOT}/Boost/)
set(BOOST_URL https://github.com/boostorg/boost/releases/download/boost-1.85.0/boost-1.85.0-cmake.tar.gz)

fetchcontent_declare(
    Boost
    OVERRIDE_FIND_PACKAGE
    EXCLUDE_FROM_ALL
    INSTALL_COMMAND ""
    TEST_COMMAND ""
    URL ${BOOST_URL}
    URL_HASH MD5=b21c059b592a041e90ae328fc5a8861b
    DOWNLOAD_DIR      ${THIRDPARTY_DOWNLOAD_ROOT}
    SOURCE_DIR        ${BOOST_ROOT})

fetchcontent_makeavailable(Boost)

message("-- Preparing Boost finished.")
