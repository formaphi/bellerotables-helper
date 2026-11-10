# protoc-gen-bvalidate: protoc plugin that turns buf.validate field options into C++ validate()
# functions, plus the small runtime those functions call.

# LEARN: the C++ of buf/validate/validate.proto serves twice: the plugin links it so protoc's
# descriptors expose the (buf.validate.field) extension, and every generated contract .pb.cc
# references its descriptor table because the contracts import it.
bellerophon_add_proto_library(buf_validate_proto
    IMPORT_DIRS ${BELLEROPHON_CONTRACTS_PROTO_DIR}/third_party
    PROTOS buf/validate/validate.proto
)

add_library(bvalidate_runtime STATIC runtime/src/runtime.cpp)
target_include_directories(bvalidate_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/runtime/include>
)
set_target_properties(bvalidate_runtime PROPERTIES POSITION_INDEPENDENT_CODE ON)

add_executable(protoc-gen-bvalidate
    src/main.cpp
    src/validate_generator.cpp
)
target_link_libraries(protoc-gen-bvalidate
    PRIVATE
        buf_validate_proto
        protobuf::libprotoc
)
