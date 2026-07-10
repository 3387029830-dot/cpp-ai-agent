#pragma once

#include "core/Message.h"

#include <filesystem>
#include <string>
#include <vector>

namespace cpp_ai_agent::storage {

struct HistoryFile {
    std::filesystem::path path;
    std::uintmax_t size = 0;
    std::string modifiedTime;
    std::string title;  // session title extracted from filename, empty for old-format logs
};

/// List all .jsonl session logs in the history directory, newest first.
/// The `title` field is parsed from filenames of the form
///   session-YYYYMMDD-HHMMSS-<title>.jsonl
/// and is empty for logs created before title support was added.
std::vector<HistoryFile> listHistoryFiles(const std::filesystem::path& historyDir);

/// Replay a history file as human-readable text lines.
std::vector<std::string> replayHistoryFile(const std::filesystem::path& path);

/// Parse a JSONL session log back into a sequence of Message objects.
///
/// Only User, Assistant, and Tool messages are reconstructed; System
/// messages (including skill injections) and meta-events
/// (skill_selected, warning, etc.) are skipped so the caller can
/// provide a fresh System Prompt.
///
/// Corrupted lines are skipped with a count reported via the
/// `skippedLines` output parameter.
std::vector<core::Message> loadMessagesFromHistory(
    const std::filesystem::path& path,
    int* skippedLines = nullptr);

}  // namespace cpp_ai_agent::storage
