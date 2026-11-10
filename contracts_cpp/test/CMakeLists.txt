find_package(GTest REQUIRED)

# LEARN: a second caller of the same command, proving it is reusable: the fixture imports a
# contract message, links contracts_cpp, and recurses into its generated validators.
bellerophon_add_proto_library(bvalidate_fixture_proto
    IMPORT_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/proto
        ${BELLEROPHON_CONTRACTS_PROTO_DIR}
        ${BELLEROPHON_CONTRACTS_PROTO_DIR}/third_party
    PROTOS bvalidate/fixture/tree.proto
    VALIDATE
    LINK_LIBRARIES contracts_cpp
)

file(GLOB CONTRACTS_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_LIST_DIR}/*.cpp")

# LEARN: every test binary that runs protobuf code links this, so Valgrind does not report
# protobuf's process-lifetime globals as "possibly lost" (see support/protobuf_shutdown_listener.cpp).
# OBJECT keeps the static listener registration from being dropped; PRIVATE protobuf keeps its
# headers off the include path of the binaries that link it.
add_library(protobuf_test_shutdown OBJECT support/protobuf_shutdown_listener.cpp)
target_link_libraries(protobuf_test_shutdown
    PRIVATE
        protobuf::libprotobuf
        GTest::gtest
)

add_executable(contracts_cpp_tests ${CONTRACTS_TEST_SOURCES})
target_link_libraries(contracts_cpp_tests
    PRIVATE
        contracts_cpp
        bvalidate_fixture_proto
        protobuf_test_shutdown
        GTest::gtest
        GTest::gmock_main
)
add_test(NAME contracts_cpp_tests COMMAND contracts_cpp_tests)

# LEARN: fail-closed check. Each proto uses a rule the generator does not implement; code
# generation must fail with a message naming it, never emit a validator that skips the rule.
set(UNSUPPORTED_DIR ${CMAKE_CURRENT_SOURCE_DIR}/proto_unsupported)
set(UNSUPPORTED_OUT ${CMAKE_CURRENT_BINARY_DIR}/unsupported_out)
file(MAKE_DIRECTORY ${UNSUPPORTED_OUT})
foreach(case_and_pattern
        "unsupported_rule|unsupported buf.validate rule\\(s\\): int32.lte"
        "required_rule|unsupported buf.validate rule\\(s\\): required"
        "message_rule|bvalidate.unsupported.M: message-level buf.validate rules are not supported"
        "repeated_rule|tags: string rules on a repeated/map field"
        "type_mismatch|big: int32 rules on a field of type int64")
    string(REPLACE "|" ";" parts "${case_and_pattern}")
    list(GET parts 0 case)
    list(GET parts 1 pattern)
    add_test(NAME bvalidate_rejects_${case}
        COMMAND $<TARGET_FILE:protobuf::protoc>
            -I${UNSUPPORTED_DIR} -I${BELLEROPHON_CONTRACTS_PROTO_DIR}/third_party
            "-I$<JOIN:$<REMOVE_DUPLICATES:$<TARGET_PROPERTY:protobuf::libprotobuf,INTERFACE_INCLUDE_DIRECTORIES>>,$<SEMICOLON>-I>"
            --plugin=protoc-gen-bvalidate=$<TARGET_FILE:protoc-gen-bvalidate>
            --bvalidate_out=${UNSUPPORTED_OUT}
            ${case}.proto
        COMMAND_EXPAND_LISTS
    )
    # LEARN: no_memcheck keeps these out of `make test-valgrind`. ctest would wrap the prebuilt
    # Conan protoc, not our plugin (Valgrind does not follow child processes), and
    # PASS_REGULAR_EXPRESSION ignores the exit code, so memcheck only added protoc's noise.
    set_tests_properties(bvalidate_rejects_${case}
        PROPERTIES
            PASS_REGULAR_EXPRESSION "${pattern}"
            LABELS no_memcheck
    )
endforeach()
