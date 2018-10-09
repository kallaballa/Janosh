# Install script for directory: /home/elchaschab/devel/Janosh/libsocket-2.4.1/headers

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "/usr/local")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Install shared libraries without execute permission?
if(NOT DEFINED CMAKE_INSTALL_SO_NO_EXE)
  set(CMAKE_INSTALL_SO_NO_EXE "0")
endif()

if(NOT CMAKE_INSTALL_COMPONENT OR "${CMAKE_INSTALL_COMPONENT}" STREQUAL "Unspecified")
  list(APPEND CMAKE_ABSOLUTE_DESTINATION_FILES
   "/usr/include/libsocket/unixdgram.hpp;/usr/include/libsocket/exception.hpp;/usr/include/libsocket/inetclientdgram.hpp;/usr/include/libsocket/libinetsocket.h;/usr/include/libsocket/unixserverstream.hpp;/usr/include/libsocket/dgramclient.hpp;/usr/include/libsocket/streamclient.hpp;/usr/include/libsocket/inetserverstream.hpp;/usr/include/libsocket/unixclientdgram.hpp;/usr/include/libsocket/socket.hpp;/usr/include/libsocket/inetbase.hpp;/usr/include/libsocket/inetserverdgram.hpp;/usr/include/libsocket/unixclientstream.hpp;/usr/include/libsocket/libunixsocket.h;/usr/include/libsocket/select.hpp;/usr/include/libsocket/inetclientstream.hpp;/usr/include/libsocket/unixbase.hpp;/usr/include/libsocket/unixserverdgram.hpp;/usr/include/libsocket/inetdgram.hpp;/usr/include/libsocket/epoll.hpp")
  if(CMAKE_WARN_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(WARNING "ABSOLUTE path INSTALL DESTINATION : ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
  if(CMAKE_ERROR_ON_ABSOLUTE_INSTALL_DESTINATION)
    message(FATAL_ERROR "ABSOLUTE path INSTALL DESTINATION forbidden (by caller): ${CMAKE_ABSOLUTE_DESTINATION_FILES}")
  endif()
file(INSTALL DESTINATION "/usr/include/libsocket" TYPE FILE FILES
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./unixdgram.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./exception.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./inetclientdgram.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./libinetsocket.h"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./unixserverstream.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./dgramclient.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./streamclient.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./inetserverstream.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./unixclientdgram.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./socket.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./inetbase.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./inetserverdgram.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./unixclientstream.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./libunixsocket.h"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./select.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./inetclientstream.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./unixbase.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./unixserverdgram.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./inetdgram.hpp"
    "/home/elchaschab/devel/Janosh/libsocket-2.4.1/headers/./epoll.hpp"
    )
endif()

