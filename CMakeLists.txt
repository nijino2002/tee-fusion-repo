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
find_library(OPENSSL_SSL ssl) ; find_library(OPENSSL_CRYPTO crypto)
if(OPENSSL_SSL AND OPENSSL_CRYPTO) target_link_libraries(tee_fusion ${OPENSSL_SSL} ${OPENSSL_CRYPTO}) endif()
add_executable(ratls_server examples/ratls/server.c)
add_executable(ratls_client examples/ratls/client.c)
target_link_libraries(ratls_server tee_fusion ${OPENSSL_SSL} ${OPENSSL_CRYPTO})
target_link_libraries(ratls_client tee_fusion ${OPENSSL_SSL} ${OPENSSL_CRYPTO})
set_target_properties(ratls_server PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set_target_properties(ratls_client PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
enable_testing()
add_subdirectory(tests)
