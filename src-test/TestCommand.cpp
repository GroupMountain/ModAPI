#include "Global.h"
#include <ll/api/command/CommandHandle.h>
#include <ll/api/command/CommandRegistrar.h>
#include <mc/server/commands/CommandOutput.h>
#include <unordered_map>


// The command parameter type has to have external linkage: boost::pfr reflects over it to build the
// command overload.
enum class TestNames {};

struct TestCommandParam {
    ll::command::SoftEnum<TestNames> name;
};

namespace {

std::unordered_map<std::string, std::function<bool()>> testFunctions;

void runTestCommand(CommandOrigin const&, CommandOutput& output, TestCommandParam const& param) {
    auto it = testFunctions.find(param.name);
    if (it == testFunctions.end()) {
        output.error("unknown test {}", param.name);
        return;
    }
    try {
        if (it->second()) {
            output.success("test {} success", param.name);
        } else {
            output.error("test {} fail", param.name);
        }
    } catch (std::exception const& e) {
        output.error("test {} failed with exception {}", param.name, e.what());
    } catch (...) {
        output.error("test {} failed with an unknown exception", param.name);
    }
}

} // namespace

void registerTestFunction(std::string const& name, std::function<bool()> const& callback) {
    // Suites register while the mod loads, i.e. before the command system exists, so only the table
    // is filled here; the command and its soft enum values are created once the server started.
    testFunctions.insert_or_assign(name, callback);
}

void initTestCommand() {
    ll::command::CommandRegistrar& registrar = ll::command::CommandRegistrar::getInstance(false);
    registrar.getOrCreateCommand("modapitest", "ModAPI test")
        .overload<TestCommandParam>()
        .required("name")
        // A lambda (rather than the function pointer) because `execute` stores the callable type.
        .execute([](CommandOrigin const& origin, CommandOutput& output, TestCommandParam const& param) {
            runTestCommand(origin, output, param);
        });

    std::vector<std::string> names;
    names.reserve(testFunctions.size());
    for (auto const& [name, callback] : testFunctions) {
        (void)callback;
        names.push_back(name);
    }
    registrar.addSoftEnumValues("ModApiTestNames", names);
}

REGISTER_TEST(TestCommand, initTestCommand)
