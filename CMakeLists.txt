cmake_minimum_required(VERSION 3.15)
project(tee_fusion C)
set(CMAKE_C_STANDARD 11)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

option(TEE_PLATFORM "tdx|optee|keystone" "tdx")

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
  if(TEEC_INCLUDE_DIR)
    message(STATUS "Found tee_client_api.h in: ${TEEC_INCLUDE_DIR}")
    target_include_directories(tee_fusion PRIVATE ${TEEC_INCLUDE_DIR})
  else()
    message(WARNING "OP-TEE client headers (tee_client_api.h) not found; set -DTEEC_INCLUDE_DIR=...")
  endif()
  if(TEEC_LIB)
    message(STATUS "Found libteec: ${TEEC_LIB}")
    target_link_libraries(tee_fusion ${TEEC_LIB})
  else()
    message(WARNING "libteec not found; set -DTEEC_LIB=/path/to/libteec.so")
  endif()
endif()

add_executable(ratls_server examples/ratls/server.c)
add_executable(ratls_client examples/ratls/client.c)
target_link_libraries(ratls_server tee_fusion ${OPENSSL_SSL} ${OPENSSL_CRYPTO})
target_link_libraries(ratls_client tee_fusion ${OPENSSL_SSL} ${OPENSSL_CRYPTO})

set_target_properties(ratls_server PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(ratls_client PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

enable_testing()
add_subdirectory(tests)
