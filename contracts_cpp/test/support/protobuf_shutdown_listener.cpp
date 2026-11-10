#include <google/protobuf/message_lite.h>
#include <gtest/gtest.h>

/* LEARN: protobuf keeps its generated descriptor pool and default instances alive for the whole
   process and frees them only in ShutdownProtobufLibrary(). The blocks are referenced through
   interior pointers (absl hash-table control bytes, RepeatedField elements, tagged string
   pointers), so Valgrind reports them as "possibly lost", and --error-exitcode=1 turns that into
   a failed `make test-valgrind` even with no leak in our code.
   Linked as an OBJECT library: the static registration below is never dropped by the linker, and
   protobuf stays PRIVATE to this one file, so request_parser_tests still cannot include it
   (PrivateLinkTest.ProtobufIsPrivate).
   OnTestProgramEnd, not Environment::TearDown: with --gtest_repeat=-1 or
   --gtest_recreate_environments_when_repeating the environment is torn down after every
   iteration, and protobuf cannot be used again once it is shut down.
*/
namespace {

class ProtobufShutdownListener : public ::testing::EmptyTestEventListener
{
public:
    void OnTestProgramEnd(const ::testing::UnitTest& /*unitTest*/) override
    {
        google::protobuf::ShutdownProtobufLibrary();
    }
};

const bool REGISTERED = [] {
    ::testing::UnitTest::GetInstance()->listeners().Append(new ProtobufShutdownListener);
    return true;
}();

} // namespace
