include_guard(GLOBAL)

# LEARN: protoc comes from the Conan tool_requires (protobuf/<host_version>), so the generated code
# always matches the libprotobuf it links against. The contracts repo's Docker protoc (v25.1) is
# only for its own structural tests; it is never used here.
if(NOT TARGET protobuf::protoc)
    find_package(protobuf CONFIG REQUIRED)
endif()

set(BELLEROPHON_BVALIDATE_PLUGIN_TARGET protoc-gen-bvalidate)

#[=======================================================================[
bellerophon_add_proto_library(<target>
    IMPORT_DIRS <dir>...          protoc -I roots
    PROTOS <import-path>...       files to compile, named as an `import` would name them
                                  (e.g. bellerophon/ngfw/shared/logging.proto)
    [VALIDATE]                    also emit <name>.pb.validate.{h,cc} from buf.validate options
    [LINK_LIBRARIES <lib>...]     libraries holding the C++ of protos imported but not listed
)

Creates a STATIC library <target> with the generated .pb.cc (+ .pb.validate.cc) files; the
generated headers are on its PUBLIC include path, mirroring the .proto layout.
#]=======================================================================]
function(bellerophon_add_proto_library target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "VALIDATE" "" "IMPORT_DIRS;PROTOS;LINK_LIBRARIES")

    if(arg_UNPARSED_ARGUMENTS)
        message(FATAL_ERROR "bellerophon_add_proto_library(${target}): unknown arguments: ${arg_UNPARSED_ARGUMENTS}")
    endif()
    if(NOT arg_IMPORT_DIRS OR NOT arg_PROTOS)
        message(FATAL_ERROR "bellerophon_add_proto_library(${target}): IMPORT_DIRS and PROTOS are required")
    endif()

    set(out_dir "${CMAKE_CURRENT_BINARY_DIR}/${target}_generated")
    file(MAKE_DIRECTORY "${out_dir}")

    set(import_dirs "")
    set(include_flags "")
    foreach(dir IN LISTS arg_IMPORT_DIRS)
        cmake_path(ABSOLUTE_PATH dir BASE_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" NORMALIZE)
        list(APPEND import_dirs "${dir}")
        list(APPEND include_flags "-I${dir}")
    endforeach()
    # LEARN: google/protobuf/*.proto (imported by buf/validate/validate.proto) ship in the package include dir.
    list(APPEND include_flags "-I$<JOIN:$<REMOVE_DUPLICATES:$<TARGET_PROPERTY:protobuf::libprotobuf,INTERFACE_INCLUDE_DIRECTORIES>>,$<SEMICOLON>-I>")

    set(proto_files "")
    set(outputs "")
    foreach(proto IN LISTS arg_PROTOS)
        set(resolved "")
        foreach(dir IN LISTS import_dirs)
            if(EXISTS "${dir}/${proto}")
                set(resolved "${dir}/${proto}")
                break()
            endif()
        endforeach()
        if(NOT resolved)
            message(FATAL_ERROR "bellerophon_add_proto_library(${target}): ${proto} not found under IMPORT_DIRS")
        endif()
        list(APPEND proto_files "${resolved}")

        string(REGEX REPLACE "\\.proto$" "" stem "${proto}")
        list(APPEND outputs "${out_dir}/${stem}.pb.h" "${out_dir}/${stem}.pb.cc")
        if(arg_VALIDATE)
            list(APPEND outputs "${out_dir}/${stem}.pb.validate.h" "${out_dir}/${stem}.pb.validate.cc")
        endif()
    endforeach()

    set(plugin_flags "")
    set(plugin_depends "")
    if(arg_VALIDATE)
        set(plugin "${BELLEROPHON_BVALIDATE_PLUGIN_TARGET}")
        if(NOT TARGET ${plugin})
            message(FATAL_ERROR "bellerophon_add_proto_library(${target}): VALIDATE needs the ${plugin} target; add tools/protoc-gen-bvalidate first")
        endif()
        list(APPEND plugin_flags "--plugin=protoc-gen-bvalidate=$<TARGET_FILE:${plugin}>" "--bvalidate_out=${out_dir}")
        list(APPEND plugin_depends ${plugin})
    endif()

    # LEARN: PROTOS are passed as import paths, not disk paths. protoc then names each file exactly
    # as an `import` does, so nested roots (proto/ and proto/third_party/) can never register the
    # same file twice under two names ("already defined" errors).
    add_custom_command(
        OUTPUT ${outputs}
        COMMAND protobuf::protoc ${include_flags} "--cpp_out=${out_dir}" ${plugin_flags} ${arg_PROTOS}
        DEPENDS ${proto_files} protobuf::protoc ${plugin_depends}
        WORKING_DIRECTORY "${out_dir}"
        COMMENT "protoc: generating C++ for ${target}"
        COMMAND_EXPAND_LISTS
        VERBATIM
    )

    add_library(${target} STATIC ${outputs})
    target_include_directories(${target} PUBLIC "$<BUILD_INTERFACE:${out_dir}>")
    target_link_libraries(${target} PUBLIC protobuf::libprotobuf ${arg_LINK_LIBRARIES})
    if(arg_VALIDATE)
        target_link_libraries(${target} PUBLIC bvalidate_runtime)
    endif()
    set_target_properties(${target} PROPERTIES POSITION_INDEPENDENT_CODE ON)
endfunction()
