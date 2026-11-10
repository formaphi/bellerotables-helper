# C++ message classes + generated validators for the contracts submodule (protobuf SSOT).

file(GLOB_RECURSE CONTRACT_PROTOS
    RELATIVE ${BELLEROPHON_CONTRACTS_PROTO_DIR}
    CONFIGURE_DEPENDS
    ${BELLEROPHON_CONTRACTS_PROTO_DIR}/bellerophon/*.proto
)

bellerophon_add_proto_library(contracts_cpp
    IMPORT_DIRS ${BELLEROPHON_CONTRACTS_PROTO_DIR} ${BELLEROPHON_CONTRACTS_PROTO_DIR}/third_party
    PROTOS ${CONTRACT_PROTOS}
    VALIDATE
    LINK_LIBRARIES buf_validate_proto
)

add_library(bellerophon::contracts ALIAS contracts_cpp)

if(BUILD_TESTING)
    add_subdirectory(test)
endif()
