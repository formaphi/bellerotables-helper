#include "validate_generator.h"

#include <buf/validate/validate.pb.h>
#include <cctype>
#include <cstdint>
#include <format>
#include <google/protobuf/compiler/cpp/names.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <memory>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace bvalidate {

namespace {

namespace pb = google::protobuf;

class UnsupportedRule : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

/* LEARN: the supported rule set. Every rule here is matched against protovalidate's CEL definition
   in the vendored validate.proto (same rule id, same message, same presence semantics). Adding a
   rule means: peel it off in extractChecks, emit it in emitFieldChecks, implement it in runtime.
*/
struct FieldChecks
{
    std::optional<int32_t> int32Gte;
    std::optional<uint64_t> stringMaxLen;
    bool stringUuid = false;

    [[nodiscard]] bool empty() const
    {
        return !int32Gte && !stringMaxLen && !stringUuid;
    }
};

void appendName(std::string& list, const std::string& name)
{
    if (!list.empty())
    {
        list += ", ";
    }
    list += name;
}

std::string describeSetFields(const pb::Message& message, const std::string& prefix)
{
    const pb::Reflection* reflection = message.GetReflection();
    std::vector<const pb::FieldDescriptor*> fields;
    reflection->ListFields(message, &fields);

    std::string result;
    for (const pb::FieldDescriptor* field : fields)
    {
        const std::string leaf = field->is_extension() ? std::format("({})", field->full_name())
                                                       : std::string(field->name());
        const std::string name = prefix.empty() ? leaf : std::format("{}.{}", prefix, leaf);
        const bool isSingularMessage =
            field->cpp_type() == pb::FieldDescriptor::CPPTYPE_MESSAGE && !field->is_repeated();
        const std::string nested =
            isSingularMessage ? describeSetFields(reflection->GetMessage(message, field), name)
                              : "";
        appendName(result, nested.empty() ? name : nested);
    }
    if (!reflection->GetUnknownFields(message).empty())
    {
        appendName(result, std::format("{}<field unknown to the plugin's validate.proto>", prefix));
    }
    return result;
}

void requireSingular(const pb::FieldDescriptor* field, pb::FieldDescriptor::Type expected,
                     const char* ruleKind)
{
    if (field->is_repeated())
    {
        throw UnsupportedRule(std::format(
            "{}: {} rules on a repeated/map field need repeated.items / map.values, which "
            "protoc-gen-bvalidate does not support",
            field->full_name(), ruleKind));
    }
    if (field->type() != expected)
    {
        throw UnsupportedRule(std::format("{}: {} rules on a field of type {}", field->full_name(),
                                          ruleKind, field->type_name()));
    }
}

FieldChecks extractChecks(const pb::FieldDescriptor* field)
{
    FieldChecks checks;
    if (!field->options().HasExtension(buf::validate::field))
    {
        return checks;
    }
    buf::validate::FieldRules remaining = field->options().GetExtension(buf::validate::field);

    if (remaining.has_int32())
    {
        requireSingular(field, pb::FieldDescriptor::TYPE_INT32, "int32");
        buf::validate::Int32Rules& rules = *remaining.mutable_int32();
        if (rules.has_gte())
        {
            checks.int32Gte = rules.gte();
            rules.clear_gte();
        }
        if (rules.ByteSizeLong() == 0)
        {
            remaining.clear_int32();
        }
    }

    if (remaining.has_string())
    {
        requireSingular(field, pb::FieldDescriptor::TYPE_STRING, "string");
        buf::validate::StringRules& rules = *remaining.mutable_string();
        if (rules.has_max_len())
        {
            checks.stringMaxLen = rules.max_len();
            rules.clear_max_len();
        }
        if (rules.has_uuid())
        {
            checks.stringUuid = rules.uuid();
            rules.clear_uuid();
        }
        if (rules.ByteSizeLong() == 0)
        {
            remaining.clear_string();
        }
    }

    if (remaining.has_ignore() && remaining.ignore() == buf::validate::IGNORE_UNSPECIFIED)
    {
        remaining.clear_ignore();
    }

    if (remaining.ByteSizeLong() != 0)
    {
        throw UnsupportedRule(
            std::format("{}: unsupported buf.validate rule(s): {} (extend protoc-gen-bvalidate)",
                        field->full_name(), describeSetFields(remaining, "")));
    }
    return checks;
}

void rejectMessageAndOneofRules(const pb::Descriptor* message)
{
    if (message->options().HasExtension(buf::validate::message))
    {
        throw UnsupportedRule(std::format("{}: message-level buf.validate rules are not supported",
                                          message->full_name()));
    }
    for (int index = 0; index < message->oneof_decl_count(); ++index)
    {
        const pb::OneofDescriptor* oneof = message->oneof_decl(index);
        if (oneof->options().HasExtension(buf::validate::oneof))
        {
            throw UnsupportedRule(
                std::format("{}: oneof buf.validate rules are not supported", oneof->full_name()));
        }
    }
}

bool hasRules(const pb::Descriptor* message, std::set<const pb::Descriptor*>& visited)
{
    if (!visited.insert(message).second)
    {
        return false;
    }
    rejectMessageAndOneofRules(message);
    for (int index = 0; index < message->field_count(); ++index)
    {
        const pb::FieldDescriptor* field = message->field(index);
        if (!extractChecks(field).empty())
        {
            return true;
        }
        if (field->message_type() != nullptr && hasRules(field->message_type(), visited))
        {
            return true;
        }
    }
    return false;
}

bool hasRules(const pb::Descriptor* message)
{
    std::set<const pb::Descriptor*> visited;
    return hasRules(message, visited);
}

std::string stem(const pb::FileDescriptor* file)
{
    return pb::compiler::cpp::StripProto(file->name());
}

// LEARN: names.h is protoc's public naming API for plugins; using it keeps namespaces, nested
// class names and keyword-mangled accessors identical to what --cpp_out generated. (helpers.h
// and io/printer.h pull in absl containers that GCC 13 rejects under -fsanitize=undefined.)
std::string cppNamespace(const pb::FileDescriptor* file)
{
    std::string namespaceName = pb::compiler::cpp::Namespace(file);
    return namespaceName.starts_with("::") ? namespaceName.substr(2) : namespaceName;
}

std::string qualifiedFunction(const pb::Descriptor* message, const char* function)
{
    const std::string namespaceName = cppNamespace(message->file());
    return namespaceName.empty() ? std::format("::{}", function)
                                 : std::format("::{}::{}", namespaceName, function);
}

std::string includeGuard(const pb::FileDescriptor* file)
{
    std::string guard = "BVALIDATE_";
    for (const char character : stem(file) + "_pb_validate_h")
    {
        guard += std::isalnum(static_cast<unsigned char>(character)) != 0
                     ? static_cast<char>(std::toupper(static_cast<unsigned char>(character)))
                     : '_';
    }
    return guard;
}

std::string int32Literal(int32_t value)
{
    return std::format("static_cast<int32_t>({}LL)", value);
}

std::string mapKeyExpression(const pb::FieldDescriptor* key)
{
    switch (key->cpp_type())
    {
        case pb::FieldDescriptor::CPPTYPE_STRING:
        case pb::FieldDescriptor::CPPTYPE_BOOL:
            return "key";
        case pb::FieldDescriptor::CPPTYPE_INT32:
        case pb::FieldDescriptor::CPPTYPE_INT64:
            return "static_cast<int64_t>(key)";
        default:
            return "static_cast<uint64_t>(key)";
    }
}

std::string indent(const std::string& lines, const std::string& prefix)
{
    std::string result;
    for (std::size_t start = 0; start < lines.size();)
    {
        const std::size_t end = lines.find('\n', start);
        result += prefix + lines.substr(start, end - start + 1);
        start = end + 1;
    }
    return result;
}

class FileEmitter
{
public:
    explicit FileEmitter(const pb::FileDescriptor* file)
        : m_file(file)
    {
        for (int index = 0; index < file->message_type_count(); ++index)
        {
            collectMessages(file->message_type(index));
        }
        for (const pb::Descriptor* message : m_messages)
        {
            emitDefinition(message);
        }
    }

    [[nodiscard]] std::string header() const
    {
        std::string out = std::format(
            "// Generated by protoc-gen-bvalidate from {}. DO NOT EDIT.\n"
            "#ifndef {}\n#define {}\n\n"
            "#include \"{}.pb.h\"\n#include \"bvalidate/runtime.h\"\n\n#include <string>\n\n",
            m_file->name(), includeGuard(m_file), includeGuard(m_file), stem(m_file));
        out += openNamespace();
        for (const pb::Descriptor* message : m_messages)
        {
            const std::string type = pb::compiler::cpp::QualifiedClassName(message);
            out += std::format(
                "[[nodiscard]] ::bvalidate::Violations validate(const {}& message);\n"
                "void collectViolations(const {}& message, const std::string& path,\n"
                "                       ::bvalidate::Violations& violations);\n\n",
                type, type);
        }
        out += closeNamespace();
        out += std::format("#endif // {}\n", includeGuard(m_file));
        return out;
    }

    [[nodiscard]] std::string source() const
    {
        std::string out = std::format(
            "// Generated by protoc-gen-bvalidate from {}. DO NOT EDIT.\n"
            "#include \"{}.pb.validate.h\"\n",
            m_file->name(), stem(m_file));
        for (const pb::FileDescriptor* dependency : m_includedFiles)
        {
            out += std::format("#include \"{}.pb.validate.h\"\n", stem(dependency));
        }
        out += "\n#include <cstddef>\n#include <cstdint>\n\n";
        out += openNamespace();
        out += m_definitions;
        out += closeNamespace();
        return out;
    }

private:
    const pb::FileDescriptor* m_file;
    std::vector<const pb::Descriptor*> m_messages;
    std::set<const pb::FileDescriptor*> m_includedFiles;
    std::string m_definitions;

    void collectMessages(const pb::Descriptor* message)
    {
        if (message->options().map_entry())
        {
            return;
        }
        rejectMessageAndOneofRules(message);
        m_messages.push_back(message);
        for (int index = 0; index < message->nested_type_count(); ++index)
        {
            collectMessages(message->nested_type(index));
        }
    }

    [[nodiscard]] std::string openNamespace() const
    {
        const std::string namespaceName = cppNamespace(m_file);
        return namespaceName.empty() ? "" : std::format("namespace {} {{\n\n", namespaceName);
    }

    [[nodiscard]] std::string closeNamespace() const
    {
        const std::string namespaceName = cppNamespace(m_file);
        return namespaceName.empty() ? "" : std::format("}} // namespace {}\n\n", namespaceName);
    }

    std::string recurseCall(const pb::Descriptor* target, const std::string& value,
                            const std::string& path)
    {
        if (target->file() != m_file)
        {
            m_includedFiles.insert(target->file());
        }
        return std::format("{}({}, {}, violations);",
                           qualifiedFunction(target, "collectViolations"), value, path);
    }

    std::string emitRecursion(const pb::FieldDescriptor* field)
    {
        const pb::Descriptor* target = field->is_map()
                                           ? field->message_type()->map_value()->message_type()
                                           : field->message_type();
        // LEARN: a referenced type from another file is validated through that file's
        // .pb.validate.h. If nobody generated it (no VALIDATE on its library), the include fails
        // to compile: the build breaks instead of silently skipping the nested rules.
        if (target == nullptr || !hasRules(target))
        {
            return "";
        }
        const std::string accessor = pb::compiler::cpp::FieldName(field);
        const std::string& name = field->name();
        if (field->is_map())
        {
            const std::string key = mapKeyExpression(field->message_type()->map_key());
            return std::format(
                "    for (const auto& [key, value] : message.{}())\n    {{\n        {}\n    }}\n",
                accessor,
                recurseCall(target, "value",
                            std::format("::bvalidate::keyPath(path, \"{}\", {})", name, key)));
        }
        if (field->is_repeated())
        {
            return std::format(
                "    for (int index = 0; index < message.{}_size(); ++index)\n    {{\n        {}\n"
                "    }}\n",
                accessor,
                recurseCall(target, std::format("message.{}(index)", accessor),
                            std::format("::bvalidate::indexPath(path, \"{}\", "
                                        "static_cast<std::size_t>(index))",
                                        name)));
        }
        return std::format("    if (message.has_{}())\n    {{\n        {}\n    }}\n", accessor,
                           recurseCall(target, std::format("message.{}()", accessor),
                                       std::format("::bvalidate::fieldPath(path, \"{}\")", name)));
    }

    static std::string emitFieldChecks(const pb::FieldDescriptor* field)
    {
        const FieldChecks checks = extractChecks(field);
        if (checks.empty())
        {
            return "";
        }
        const std::string accessor = pb::compiler::cpp::FieldName(field);
        const std::string value = std::format("message.{}()", accessor);
        const std::string path = std::format("::bvalidate::fieldPath(path, \"{}\")", field->name());

        std::string body;
        if (checks.int32Gte)
        {
            body += std::format("::bvalidate::checkInt32Gte({}, {}, {}, violations);\n", value,
                                int32Literal(*checks.int32Gte), path);
        }
        if (checks.stringMaxLen)
        {
            body += std::format("::bvalidate::checkStringMaxLen({}, {}ULL, {}, violations);\n",
                                value, *checks.stringMaxLen, path);
        }
        if (checks.stringUuid)
        {
            body += std::format("::bvalidate::checkStringUuid({}, {}, violations);\n", value, path);
        }

        // LEARN: protovalidate applies rules to a field with presence only when it is set; a
        // field without presence (plain proto3 scalar) is always checked, zero value included.
        if (!field->has_presence())
        {
            return indent(body, "    ");
        }
        return std::format("    if (message.has_{}())\n    {{\n{}    }}\n", accessor,
                           indent(body, "        "));
    }

    void emitDefinition(const pb::Descriptor* message)
    {
        std::string body;
        for (int index = 0; index < message->field_count(); ++index)
        {
            const pb::FieldDescriptor* field = message->field(index);
            body += emitFieldChecks(field);
            body += emitRecursion(field);
        }

        const std::string type = pb::compiler::cpp::QualifiedClassName(message);
        m_definitions += std::format(
            "void collectViolations([[maybe_unused]] const {}& message,\n"
            "                       [[maybe_unused]] const std::string& path,\n"
            "                       [[maybe_unused]] ::bvalidate::Violations& violations)\n"
            "{{\n{}}}\n\n"
            "::bvalidate::Violations validate(const {}& message)\n"
            "{{\n"
            "    ::bvalidate::Violations violations;\n"
            "    collectViolations(message, std::string(), violations);\n"
            "    return violations;\n"
            "}}\n\n",
            type, body, type);
    }
};

struct GeneratedFile
{
    std::string name;
    std::string content;
};

void writeFile(pb::compiler::GeneratorContext* context, const GeneratedFile& file)
{
    const std::unique_ptr<pb::io::ZeroCopyOutputStream> output(context->Open(file.name));
    pb::io::CodedOutputStream stream(output.get());
    stream.WriteString(file.content);
}

} // namespace

bool ValidateGenerator::Generate(const pb::FileDescriptor* file,
                                 [[maybe_unused]] const std::string& parameter,
                                 pb::compiler::GeneratorContext* context, std::string* error) const
{
    try
    {
        const FileEmitter emitter(file);
        writeFile(context, {.name = stem(file) + ".pb.validate.h", .content = emitter.header()});
        writeFile(context, {.name = stem(file) + ".pb.validate.cc", .content = emitter.source()});
        return true;
    }
    catch (const UnsupportedRule& unsupported)
    {
        *error = unsupported.what();
        return false;
    }
}

uint64_t ValidateGenerator::GetSupportedFeatures() const
{
    return FEATURE_PROTO3_OPTIONAL;
}

} // namespace bvalidate
