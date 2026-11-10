#ifndef BVALIDATE_VALIDATE_GENERATOR_H
#define BVALIDATE_VALIDATE_GENERATOR_H

#include <cstdint>
#include <google/protobuf/compiler/code_generator.h>
#include <string>

namespace bvalidate {

/* LEARN: emits <file>.pb.validate.{h,cc} with one validate(const Msg&) per message, checking the
   buf.validate rules compiled into the descriptors. Only the rules listed in validate_generator.cpp
   are implemented; any other rule fails code generation, so a contract that outgrows the generator
   breaks the build instead of being half-validated.
*/
class ValidateGenerator final : public google::protobuf::compiler::CodeGenerator
{
public:
    bool Generate(const google::protobuf::FileDescriptor* file, const std::string& parameter,
                  google::protobuf::compiler::GeneratorContext* context,
                  std::string* error) const override;

    [[nodiscard]] uint64_t GetSupportedFeatures() const override;
};

} // namespace bvalidate

#endif // BVALIDATE_VALIDATE_GENERATOR_H
