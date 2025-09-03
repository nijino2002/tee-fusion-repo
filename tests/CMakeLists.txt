add_executable(test_cbor tests_cbor.c)
target_link_libraries(test_cbor tee_fusion)
add_test(NAME cbor COMMAND test_cbor)
