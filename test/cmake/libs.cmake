include(ExternalProject)

set(LIBURING_INSTALL_DIR "${PROJECT_SOURCE_DIR}/lib/iouring")
ExternalProject_Add(liburing
    GIT_REPOSITORY https://github.com/axboe/liburing.git
    GIT_TAG master
    UPDATE_DISCONNECTED TRUE
    CONFIGURE_COMMAND ./configure --prefix=${LIBURING_INSTALL_DIR} --cc=gcc --cxx=g++
    BUILD_COMMAND make
    INSTALL_COMMAND make install
    BUILD_IN_SOURCE TRUE
    LOG_DOWNLOAD ON
    LOG_BUILD ON
    LOG_INSTALL ON
)

set(DPDK_VERSION "23.07")
set(DPDK_INSTALL_DIR "${PROJECT_SOURCE_DIR}/lib/dpdk")
ExternalProject_Add(dpdk
    PREFIX ${DPDK_INSTALL_DIR}
    URL "https://fast.dpdk.org/rel/dpdk-${DPDK_VERSION}.tar.xz"
    CONFIGURE_COMMAND ""
    BUILD_IN_SOURCE TRUE
    BUILD_COMMAND meson setup -Dexamples=all -Dprefix=${DPDK_INSTALL_DIR} build
    INSTALL_COMMAND ninja -C build install
    LOG_DOWNLOAD ON
    LOG_BUILD ON
    LOG_INSTALL ON
)


set(ZEROMQ_INSTALL_DIR ${PROJECT_SOURCE_DIR}/../../lib/zeromq)
set(CPPZMQ_INSTALL_DIR ${PROJECT_SOURCE_DIR}/../../lib/cppzmq)

set(ENV{PKG_CONFIG_PATH} "${PROJECT_SOURCE_DIR}/lib/iouring/lib/pkgconfig:$ENV{PKG_CONFIG_PATH}")
pkg_search_module(LIBURING QUIET IMPORTED_TARGET liburing)

set(ENV{PKG_CONFIG_PATH} "${PROJECT_SOURCE_DIR}/lib/dpdk/lib/x86_64-linux-gnu/pkgconfig:$ENV{PKG_CONFIG_PATH}")
pkg_search_module(DPDK QUIET IMPORTED_TARGET libdpdk)
string(REPLACE ";" " " DPDK_CFLAGS "${DPDK_CFLAGS}")

set(ENV{PKG_CONFIG_PATH} "${ZEROMQ_INSTALL_DIR}/lib/pkgconfig:${CPPZMQ_INSTALL_DIR}/lib/pkgconfig")
pkg_check_modules(CPPZMQ QUIET IMPORTED_TARGET cppzmq)

set(ENV{PKG_CONFIG_PATH} "${ZEROMQ_INSTALL_DIR}/lib/pkgconfig")
pkg_check_modules(LIBZMQ QUIET IMPORTED_TARGET libzmq)

pkg_search_module(PROTOBUF REQUIRED protobuf)

set(LIBRARY_CFLAGS      ${DPDK_CFLAGS})
set(LIBRARY_INCLUDES    ${DPDK_INCLUDE_DIRS}    ${LIBURING_INCLUDE_DIRS} ${CPPZMQ_INCLUDE_DIRS} ${PROTOBUF_INCLUDE_DIRS})
set(LIBRARY_DIRS        ${DPDK_LIBRARY_DIRS}    ${LIBURING_LIBRARY_DIRS} ${CPPZMQ_LIBRARY_DIRS})
set(LIBRARIES           ${DPDK_LINK_LIBRARIES}  ${LIBURING_LINK_LIBRARIES} ${LIBZMQ_LINK_LIBRARIES} ${PROTOBUF_LIBRARIES})
