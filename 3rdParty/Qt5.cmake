message(STATUS "Preparing Qt5...")

set(QT_ROOT ${THIRDPARTY_INSTALL_ROOT}/Qt5/)
set(QT_ROOT_ARCH ${QT_ROOT}/msvc2017_64/)
set(QT_ARCHIVE ${THIRDPARTY_DOWNLOAD_ROOT}/Qt5_14_2_msvc2017_64.7z)
set(QT_URL https://sourceforge.net/projects/micbridge-3rdparty/files/Qt5_14_2_msvc2017_64.7z)

if (NOT EXISTS ${QT_ARCHIVE})
    message(STATUS "Downloading Qt5...")
    file(DOWNLOAD ${QT_URL} ${QT_ARCHIVE} SHOW_PROGRESS)
endif()

if (NOT EXISTS ${QT_ROOT_ARCH})
    message(STATUS "Extracting Qt5 to ${QT_ROOT}...")
    file(ARCHIVE_EXTRACT INPUT ${QT_ARCHIVE} DESTINATION ${QT_ROOT})
endif()

find_package(Qt5
             PATHS ${QT_ROOT_ARCH}
             GLOBAL
             REQUIRED
             COMPONENTS Core Widgets Multimedia)

get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

install(
    DIRECTORY ${CMAKE_BINARY_DIR}/qtDeploy/
    DESTINATION .
    FILES_MATCHING PATTERN "*.*"
)

install(
        DIRECTORY ${CMAKE_BINARY_DIR}/qtDeploy/styles
        DIRECTORY ${CMAKE_BINARY_DIR}/qtDeploy/platforms
        DIRECTORY ${CMAKE_BINARY_DIR}/qtDeploy/imageformats
        DESTINATION .
        FILES_MATCHING PATTERN "*.*"
    )

install(
        FILES ${CMAKE_BINARY_DIR}/qtDeploy/Qt5Widgets.dll
        ${CMAKE_BINARY_DIR}/qtDeploy/Qt5Gui.dll
        ${CMAKE_BINARY_DIR}/qtDeploy/Qt5Core.dll
        DESTINATION .
    )

message(STATUS "Preparing Qt5 finished.")
