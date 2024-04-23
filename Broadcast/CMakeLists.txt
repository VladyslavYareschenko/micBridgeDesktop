add_library(Broadcast_interface INTERFACE)

add_library(Broadcast::interface ALIAS Broadcast_interface)

target_include_directories(Broadcast_interface
        INTERFACE
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)

add_library(Broadcast
  src/BC_BufferedMediaSink.cpp
  src/BC_Listener.cpp
  src/BC_ListenerImpl.cpp
  src/BC_Live555Runtime.cpp
)

add_subdirectory(live555 EXCLUDE_FROM_ALL)

target_link_libraries(Broadcast
	PUBLIC Broadcast::interface
        PRIVATE live555)

target_include_directories(Broadcast
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:include>)
