#include <doctest/doctest.h>

#include "storage/HistoryReader.h"
#include "storage/JsonLogger.h"

#include <filesystem>
#include <fstream>
#include <string>

namespace {

std::filesystem::path makeTempHistoryDir(const std::string& name) {
    auto root = std::filesystem::temp_directory_path() / ("cpp-ai-agent-history-" + name);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    return root;
}

void writeLine(const std::filesystem::path& path, const std::string& line) {
    std::ofstream output(path, std::ios::binary | std::ios::app);
    output << line << '\n';
}

}  // namespace

TEST_CASE("HistoryReader lists jsonl files") {
    const auto dir = makeTempHistoryDir("list");
    writeLine(dir / "session-a.jsonl", R"({"type":"user_message","data":{"content":"hi"}})");
    writeLine(dir / "ignore.txt", "ignored");

    const auto files = cpp_ai_agent::storage::listHistoryFiles(dir);

    REQUIRE(files.size() == 1);
    CHECK(files.at(0).path.filename().string() == "session-a.jsonl");
    CHECK(files.at(0).size > 0);

    std::filesystem::remove_all(dir);
}

TEST_CASE("HistoryReader extracts titles from old and millisecond log filenames") {
    const auto dir = makeTempHistoryDir("titles");
    writeLine(dir / "session-20260710-120000-old-title.jsonl", R"({"type":"user_message","data":{"content":"hi"}})");
    writeLine(dir / "session-20260710-120001-123-new-title.jsonl", R"({"type":"user_message","data":{"content":"hi"}})");
    writeLine(dir / "session-20260710-120002-456.jsonl", R"({"type":"user_message","data":{"content":"hi"}})");
    writeLine(dir / "session-20260710-120003-789-4242-pid-title.jsonl", R"({"type":"user_message","data":{"content":"hi"}})");

    const auto files = cpp_ai_agent::storage::listHistoryFiles(dir);

    REQUIRE(files.size() == 4);
    CHECK(files.at(0).title == "pid-title");
    CHECK(files.at(1).title.empty());
    CHECK(files.at(2).title == "new-title");
    CHECK(files.at(3).title == "old-title");

    std::filesystem::remove_all(dir);
}

TEST_CASE("HistoryReader replays common log events") {
    const auto dir = makeTempHistoryDir("replay");
    const auto path = dir / "session.jsonl";
    writeLine(path, R"({"type":"user_message","data":{"content":"hello"}})");
    writeLine(path, R"({"type":"tool_call","data":{"name":"read_file","arguments":"{\"path\":\"README.md\"}"}})");
    writeLine(path, R"({"type":"tool_result","data":{"content":"file content"}})");
    writeLine(path, R"({"type":"assistant_message","data":{"content":"done"}})");

    const auto lines = cpp_ai_agent::storage::replayHistoryFile(path);

    REQUIRE(lines.size() == 4);
    CHECK(lines.at(0) == "user> hello");
    CHECK(lines.at(1).find("tool_call> read_file") == 0);
    CHECK(lines.at(2) == "tool_result> file content");
    CHECK(lines.at(3) == "assistant> done");

    std::filesystem::remove_all(dir);
}

TEST_CASE("HistoryReader loads resumable chat messages from jsonl history") {
    const auto dir = makeTempHistoryDir("load");
    const auto path = dir / "session.jsonl";
    writeLine(path, R"({"type":"user_message","timestamp":1,"data":{"content":"hello"}})");
    writeLine(path, "not json");
    writeLine(path, R"({"type":"assistant_message","timestamp":2,"data":{"content":"hi there"}})");

    int skipped = 0;
    const auto messages = cpp_ai_agent::storage::loadMessagesFromHistory(path, &skipped);

    REQUIRE(messages.size() == 2);
    CHECK(skipped == 1);
    CHECK(messages.at(0).content == "hello");
    CHECK(messages.at(1).content == "hi there");

    std::filesystem::remove_all(dir);
}
