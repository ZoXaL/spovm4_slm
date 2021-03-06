cmake_minimum_required(VERSION 3.5)
project(slm VERSION 1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 11)
set(SYSTEMD_UNIT_DIR "/usr/lib/systemd/system")

find_package(PkgConfig)
pkg_check_modules(GLIB2 glib-2.0)
pkg_check_modules(GIO2 gio-2.0)
pkg_check_modules(UDEV libudev)

include_directories(include/ include/monitors include/logging/)
include_directories(${GLIB2_INCLUDE_DIRS} ${GIO2_INCLUDE_DIRS} ${UDEV_INCLUDE_DIRS})

set(MONITOR_SRC src/monitors/monitor.c
        src/monitors/inotify_monitor.c
        src/monitors/dbus_monitor.c
        src/monitors/udev_monitor.c
        )

add_library(slm-monitor ${MONITOR_SRC})

add_executable(slm src/utility/main.c src/logging.c)
add_executable (slmd src/daemon/main.c src/logging.c)
target_compile_definitions(slmd PUBLIC -DDAEMON)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package (Threads)
target_link_libraries (slm
        slm-monitor
        ${CMAKE_THREAD_LIBS_INIT}
        ${GLIB2_LIBRARIES}
        ${GIO2_LIBRARIES}
        ${UDEV_LIBRARIES})
target_link_libraries (slmd
        slm-monitor
        ${CMAKE_THREAD_LIBS_INIT}
        ${GLIB2_LIBRARIES}
        ${GIO2_LIBRARIES}
        ${UDEV_LIBRARIES})

install (TARGETS slm DESTINATION /usr/bin)
install (TARGETS slmd DESTINATION /usr/bin)

install(CODE "file( INSTALL ${PROJECT_SOURCE_DIR}/src/daemon/slmd.service
                    DESTINATION ${SYSTEMD_UNIT_DIR})")

set(CPACK_PACKAGING_INSTALL_PREFIX "/")
IF(EXISTS "${CMAKE_ROOT}/Modules/CPack.cmake")
    INCLUDE(InstallRequiredSystemLibraries)

    SET(CPACK_GENERATOR "DEB")

    SET(CPACK_PACKAGE_NAME "slm")
    SET(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}zoxal1")
    SET(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")

    SET(CPACK_PACKAGE_VENDOR "zoxal")
    SET(CPACK_DEBIAN_PACKAGE_MAINTAINER "zzoxall@gmail.com")
    SET(CPACK_DEBIAN_PACKAGE_DESCRIPTION "system linux monitor")
    SET(CPACK_DEBIAN_PACKAGE_DESCRIPTION_SUMMARY
            "slm utitity and slmd daemon aimed to monitor linux system events")
    INCLUDE(CPack)
ENDIF()