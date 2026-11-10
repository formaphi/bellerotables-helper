#include "validate_generator.h"

#include <google/protobuf/compiler/plugin.h>

int main(int argc, char* argv[])
{
    const bvalidate::ValidateGenerator generator;
    return google::protobuf::compiler::PluginMain(argc, argv, &generator);
}
