#pragma once
#include "Test.hpp"
#include <fmt/format.h>
#include <fstream>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace test {

// Pass/fail reporting shared by the suites.
//
// Results go to the server log (prefixed with the suite tag) and to a report file in the mod
// directory; every section rewrites the file, so the results of a suite survive a crash in an
// unrelated one.
class Report {
public:
    Report(std::string tag, std::string fileName) : mTag(std::move(tag)), mFileName(std::move(fileName)) {}

    // Clears the counters and failures, keeping the tag and the report file.
    void reset() {
        mSection.clear();
        mFailures.clear();
        mLogged = 0;
        mPassed = 0;
        mFailed = 0;
        mSkipped = 0;
        mFinished = false;
    }

    void pass() { ++mPassed; }

    // A case that could not run (no level, no loaded chunk, missing registry entry, ...). Reported
    // separately from the failures so a headless server is still a pass.
    void skip(std::string const& name) {
        ++mSkipped;
        getLogger().warn("{} skipped {}", mTag, name);
    }

    void isTrue(std::string const& name, bool condition, std::string const& detail = {}) {
        condition ? pass() : fail(name, detail.empty() ? "condition is false" : detail);
    }

    void eq(std::string const& name, std::string_view expected, std::string_view actual) {
        expected == actual ? pass() : fail(name, fmt::format("expected [{}] but got [{}]", expected, actual));
    }

    void noThrow(std::string const& name, std::function<void()> const& body) {
        try {
            body();
            pass();
        } catch (std::exception const& e) {
            fail(name, fmt::format("threw std::exception: {}", e.what()));
        } catch (...) {
            fail(name, "threw an unknown exception");
        }
    }

    void section(std::string const& name, std::function<void()> const& body) {
        mSection = name;
        getLogger().info("{} ---- {} ----", mTag, name);
        try {
            body();
        } catch (std::exception const& e) {
            fail("section.body", fmt::format("threw std::exception: {}", e.what()));
        } catch (...) {
            fail("section.body", "threw an unknown exception");
        }
        write();
    }

    // Writes the report file and logs the failures that were not logged yet.
    void write() {
        std::string content;
        for (auto const& failure : mFailures) {
            content += fmt::format("{} FAIL {}\n", mTag, failure);
        }
        content += fmt::format(
            "{0} {1} passed={2} failed={3} skipped={4}\n",
            mTag,
            mFinished ? "RESULT" : "RUNNING",
            mPassed,
            mFailed,
            mSkipped
        );

        std::ofstream output(
            Entry::getInstance().getSelf().getModDir() / mFileName,
            std::ios_base::out | std::ios_base::trunc
        );
        if (output.is_open()) output << content;

        for (size_t i = mLogged; i < mFailures.size(); ++i) {
            getLogger().error("{} FAIL {}", mTag, mFailures[i]);
        }
        mLogged = mFailures.size();
        getLogger().info(
            "{} {} passed={} failed={} skipped={}",
            mTag,
            mFinished ? "RESULT" : "RUNNING",
            mPassed,
            mFailed,
            mSkipped
        );
    }

    // Final write; returns the number of failed cases (0 means the suite passed).
    int finish() {
        mFinished = true;
        write();
        getLogger().info("{} ===== suite end: passed={} failed={} skipped={} =====", mTag, mPassed, mFailed, mSkipped);
        return mFailed;
    }

    [[nodiscard]] int failed() const { return mFailed; }

    [[nodiscard]] int passed() const { return mPassed; }

    [[nodiscard]] int skipped() const { return mSkipped; }

private:
    void fail(std::string const& name, std::string const& detail) {
        ++mFailed;
        mFailures.push_back(fmt::format("[{}] {}{}{}", mSection, name, detail.empty() ? "" : " :: ", detail));
    }

    std::string              mTag;
    std::string              mFileName;
    std::string              mSection;
    std::vector<std::string> mFailures;
    size_t                   mLogged   = 0;
    int                      mPassed   = 0;
    int                      mFailed   = 0;
    int                      mSkipped  = 0;
    bool                     mFinished = false;
};

} // namespace test
