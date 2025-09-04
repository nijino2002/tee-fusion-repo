cmake_minimum_required(VERSION 3.15)
project(tee_fusion C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(TEE_PLATFORM "tdx|optee|keystone" "tdx")
include(ExternalProject)

include_directories(include core core/api core/evidence core/util adapters)

set(CORE_SRCS
  core/api/fusion_api.c
  core/evidence/uevidence_builder.c
  core/evidence/mapping.c
  core/util/cbor_min.c
  core/util/cose_min.c
)

if(TEE_PLATFORM STREQUAL "tdx")
  set(ADAPTER_SRCS adapters/tdx/adapter_init.c)
elseif(TEE_PLATFORM STREQUAL "optee")
  set(ADAPTER_SRCS adapters/optee/adapter_init.c)
elseif(TEE_PLATFORM STREQUAL "keystone")
  set(ADAPTER_SRCS adapters/keystone/adapter_init.c)
else()
  message(FATAL_ERROR "bad TEE_PLATFORM")
endif()

add_library(tee_fusion STATIC ${CORE_SRCS} ${ADAPTER_SRCS})

# OpenSSL
find_library(OPENSSL_SSL ssl)
find_library(OPENSSL_CRYPTO crypto)
if(OPENSSL_SSL AND OPENSSL_CRYPTO)
  target_link_libraries(tee_fusion ${OPENSSL_SSL} ${OPENSSL_CRYPTO})
endif()

# OP-TEE client (only when TEE_PLATFORM=optee)
if(TEE_PLATFORM STREQUAL "optee")
  # Option: auto fetch/build optee_client when system lib not found
  option(OPTEE_CLIENT_AUTO_FETCH "Auto fetch/build OP-TEE client (libteec) if missing" ON)

  if(NOT TEEC_INCLUDE_DIR)
    find_path(TEEC_INCLUDE_DIR tee_client_api.h
      PATHS /usr/include /usr/local/include /usr/include/aarch64-linux-gnu
    )
  endif()
  if(NOT TEEC_LIB)
    find_library(TEEC_LIB teec
      PATHS /usr/lib /usr/local/lib /usr/lib/aarch64-linux-gnu
    )
  endif()

  if(TEEC_INCLUDE_DIR AND TEEC_LIB)
    message(STATUS "Using system OP-TEE client: ${TEEC_INCLUDE_DIR} | ${TEEC_LIB}")
    target_include_directories(tee_fusion PRIVATE ${TEEC_INCLUDE_DIR})
    target_link_libraries(tee_fusion ${TEEC_LIB})
    target_compile_definitions(tee_fusion PRIVATE HAVE_TEEC=1)
  elseif(OPTEE_CLIENT_AUTO_FETCH)
    # Fetch and build optee_client from upstream.
    set(OPTEE_CLIENT_GIT_REPO "https://github.com/OP-TEE/optee_client.git" CACHE STRING "optee_client git repo")
    set(OPTEE_CLIENT_GIT_TAG "4.0.0" CACHE STRING "optee_client git tag/branch")
    ExternalProject_Add(optee_client_ep
      GIT_REPOSITORY ${OPTEE_CLIENT_GIT_REPO}
      GIT_TAG        ${OPTEE_CLIENT_GIT_TAG}
      UPDATE_DISCONNECTED 1
      CONFIGURE_COMMAND ""
      # Build libteec for the host (no cross-compile)
      BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} CROSS_COMPILE=
      INSTALL_COMMAND ""
      BUILD_IN_SOURCE 1
    )
    ExternalProject_Get_Property(optee_client_ep SOURCE_DIR)
    set(OPTEE_CLIENT_INCLUDE_DIR ${SOURCE_DIR}/libteec/include)
    set(OPTEE_CLIENT_LIB_PATH    ${SOURCE_DIR}/out/libteec/libteec.a)
    add_library(teec STATIC IMPORTED GLOBAL)
    set_target_properties(teec PROPERTIES IMPORTED_LOCATION ${OPTEE_CLIENT_LIB_PATH})
    add_dependencies(teec optee_client_ep)
    target_include_directories(tee_fusion PRIVATE ${OPTEE_CLIENT_INCLUDE_DIR})
    target_link_libraries(tee_fusion teec)
    target_compile_definitions(tee_fusion PRIVATE HAVE_TEEC=1)
    # Ensure headers/libs are built before compiling tee_fusion
    add_dependencies(tee_fusion optee_client_ep)
    message(STATUS "Configured ExternalProject for optee_client (tag: ${OPTEE_CLIENT_GIT_TAG})")
  else()
    message(WARNING "OP-TEE client not found and auto-fetch disabled. Build will fallback to non-TEEC path.")
  endif()

  # ---- TA build integration ----
  option(OPTEE_TA_AUTO_FETCH "Auto fetch/build OP-TEE OS to get TA Dev Kit and build TA" ON)
  set(OPTEE_OS_GIT_REPO "https://github.com/OP-TEE/optee_os.git" CACHE STRING "optee_os git repo")
  set(OPTEE_OS_GIT_TAG  "4.0.0" CACHE STRING "optee_os git tag/branch")
  set(OPTEE_OS_PLATFORM "vexpress-qemu_armv8a" CACHE STRING "optee_os PLATFORM (e.g., vexpress-qemu_armv8a)")
  # TA 架构：arm32 或 arm64（QEMU‑v8 默认 arm32；若需 64 位 TA，请设置 OPTEE_TA_ARCH=arm64）
  set(OPTEE_TA_ARCH     "arm32" CACHE STRING "TA export arch (arm32|arm64)")
  set(OPTEE_TA_CROSS_COMPILE "arm-linux-gnueabihf-" CACHE STRING "TA cross prefix (arm-linux-gnueabihf-|aarch64-linux-gnu-)")
  set(OPTEE_TA_CROSS_COMPILE32 "arm-linux-gnueabihf-" CACHE STRING "32-bit ARM cross prefix for optee_os")
  set(OPTEE_TA_CROSS_COMPILE64 "aarch64-linux-gnu-" CACHE STRING "64-bit ARM cross prefix for optee_os")
  set(OPTEE_TA_DEV_KIT_DIR "" CACHE PATH "Path to TA dev kit export (overrides auto fetch)")

  if(OPTEE_TA_DEV_KIT_DIR)
    add_custom_target(optee_ta
      COMMAND ${CMAKE_MAKE_PROGRAM}
              -C ${CMAKE_SOURCE_DIR}/optee/ta
              TA_DEV_KIT_DIR=${OPTEE_TA_DEV_KIT_DIR}
              CROSS_COMPILE=${OPTEE_TA_CROSS_COMPILE}
      COMMENT "Building OP-TEE TA using provided TA_DEV_KIT_DIR"
    )
  elseif(OPTEE_TA_AUTO_FETCH)
    # Prepare optee_os build variables explicitly (avoid empty arguments)
    set(OPTEE_OS_BUILD_VARS
      PLATFORM=${OPTEE_OS_PLATFORM}
      CFG_TEE_TA_LOG_LEVEL=0
      CROSS_COMPILE=${OPTEE_TA_CROSS_COMPILE32}
      CROSS_COMPILE64=${OPTEE_TA_CROSS_COMPILE64}
    )
    if(OPTEE_TA_ARCH STREQUAL "arm64")
      list(APPEND OPTEE_OS_BUILD_VARS CFG_TA_ARM64=y)
    endif()
    ExternalProject_Add(optee_os_ep
      GIT_REPOSITORY ${OPTEE_OS_GIT_REPO}
      GIT_TAG        ${OPTEE_OS_GIT_TAG}
      UPDATE_DISCONNECTED 1
      CONFIGURE_COMMAND ""
      # Build the TA dev kit export (produces out/export-ta_arm32 or arm64)
      BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} ${OPTEE_OS_BUILD_VARS} ta_dev_kit
      INSTALL_COMMAND ""
      BUILD_IN_SOURCE 1
    )
    ExternalProject_Get_Property(optee_os_ep SOURCE_DIR)
    # Derive the out dir prefix used by optee_os: out/arm-plat-<base_platform>
    # Example: vexpress-qemu_armv8a -> vexpress
    set(_OPTEE_PLAT_BASE ${OPTEE_OS_PLATFORM})
    string(REGEX REPLACE "[-_].*$" "" _OPTEE_PLAT_BASE "${_OPTEE_PLAT_BASE}")
    set(OPTEE_TA_DEV_KIT_AUTO ${SOURCE_DIR}/out/arm-plat-${_OPTEE_PLAT_BASE}/export-ta_${OPTEE_TA_ARCH})
    message(STATUS "OP-TEE TA dev kit auto path: ${OPTEE_TA_DEV_KIT_AUTO}")
    ExternalProject_Add(optee_ta_ep
      SOURCE_DIR ${CMAKE_SOURCE_DIR}/optee/ta
      CONFIGURE_COMMAND ""
      # Run make in the TA source dir where the Makefile lives
      BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_SOURCE_DIR}/optee/ta
                    TA_DEV_KIT_DIR=${OPTEE_TA_DEV_KIT_AUTO}
                    CROSS_COMPILE=${OPTEE_TA_CROSS_COMPILE}
      INSTALL_COMMAND ""
    )
    add_dependencies(optee_ta_ep optee_os_ep)
    add_custom_target(optee_ta DEPENDS optee_ta_ep)
    message(STATUS "Configured ExternalProject for TA build (platform: ${OPTEE_OS_PLATFORM})")
  else()
    message(WARNING "OP-TEE TA build disabled (no TA dev kit and auto-fetch off)")
  endif()

  # Ensure `make clean` also removes TA build byproducts in source tree
  set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_MAKE_CLEAN_FILES
    "${CMAKE_SOURCE_DIR}/optee/ta/optee_fusion_ta.o"
    "${CMAKE_SOURCE_DIR}/optee/ta/user_ta_header.o"
    "${CMAKE_SOURCE_DIR}/optee/ta/ta_entry_a32.o"
    "${CMAKE_SOURCE_DIR}/optee/ta/ta_entry_a64.o"
    "${CMAKE_SOURCE_DIR}/optee/ta/export-ta_arm32"
    "${CMAKE_SOURCE_DIR}/optee/ta/export-ta_arm64"
  )

  # Optional convenience target: `make ta_clean` to invoke TA dev kit clean
  if(OPTEE_TA_DEV_KIT_DIR)
    add_custom_target(ta_clean
      COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_SOURCE_DIR}/optee/ta
              TA_DEV_KIT_DIR=${OPTEE_TA_DEV_KIT_DIR}
              clean
      COMMENT "Cleaning OP-TEE TA artifacts (using provided TA_DEV_KIT_DIR)"
    )
  elseif(TARGET optee_os_ep)
    # Use auto-detected dev kit path for cleaning
    add_custom_target(ta_clean
      COMMAND ${CMAKE_MAKE_PROGRAM} -C ${CMAKE_SOURCE_DIR}/optee/ta
              TA_DEV_KIT_DIR=${OPTEE_TA_DEV_KIT_AUTO}
              clean
      COMMENT "Cleaning OP-TEE TA artifacts (using auto TA dev kit)"
      DEPENDS optee_os_ep
    )
  endif()

endif()

add_executable(ratls_server examples/ratls/server.c)
add_executable(ratls_client examples/ratls/client.c)
target_link_libraries(ratls_server tee_fusion ${OPENSSL_SSL} ${OPENSSL_CRYPTO})
target_link_libraries(ratls_client tee_fusion ${OPENSSL_SSL} ${OPENSSL_CRYPTO})

# OP-TEE smoke example (only build on optee platform)
if(TEE_PLATFORM STREQUAL "optee")
  add_executable(optee_smoke examples/optee_smoke/smoke.c)
  target_link_libraries(optee_smoke tee_fusion ${OPENSSL_SSL} ${OPENSSL_CRYPTO})
  set_target_properties(optee_smoke PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(TARGET optee_ta)
    add_dependencies(optee_smoke optee_ta)
  endif()
endif()

set_target_properties(ratls_server PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(ratls_client PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# If TA build target exists, make examples depend on it (after targets are defined)
if(TARGET optee_ta)
  add_dependencies(ratls_server optee_ta)
  add_dependencies(ratls_client optee_ta)
endif()

enable_testing()
add_subdirectory(tests)
