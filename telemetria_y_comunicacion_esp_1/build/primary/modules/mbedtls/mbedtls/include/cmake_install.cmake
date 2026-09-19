# Install script for directory: C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/Zephyr-Kernel")
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

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "TRUE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "C:/Users/razie/zephyr-sdk-1.0.1/gnu/xtensa-espressif_esp32_zephyr-elf/bin/xtensa-espressif_esp32_zephyr-elf-objdump.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/build_info.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/debug.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/error.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/mbedtls_config.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/net_sockets.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/oid.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/pkcs7.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/ssl.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/ssl_cache.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/ssl_ciphersuites.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/ssl_cookie.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/ssl_ticket.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/timing.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/version.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/x509.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/x509_crl.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/x509_crt.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/x509_csr.h"
    )
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/mbedtls/private" TYPE FILE PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_READ FILES
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/private/config_adjust_ssl.h"
    "C:/zephyr/zephyresp32/deps/modules/crypto/mbedtls/include/mbedtls/private/config_adjust_x509.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Users/razie/OneDrive/Escritorio/proyecto_mantenimiento/telemetria_y_comunicacion_esp_1/build/primary/modules/mbedtls/mbedtls/include/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
