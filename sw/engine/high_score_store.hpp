/// @file
/// The high-score store: the in-memory TBL-HighScore (top 10 per game), loaded from and saved
/// atomically to HISCORE_FILE (SR-FW-08, SR-FW-09, SR-FW-10, SC-03, SC-04).
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "arcade/tables.hpp"
#include "engine/file_system.hpp"

namespace arcade {

/// Number of games with a high-score table in release 1.0 (TBL-GameRegistry game_id 0-1).
constexpr std::size_t kHighScoreGameCount = kGameIdBreakout + 1;

/// Result of HighScoreStore::save (SR-FW-09).
enum class SaveStatus : std::uint8_t {
    Ok,
    OpenFailed,
    WriteFailed,
    SyncFailed,
    CloseFailed,
    RenameFailed,
};

/// TBL-HighScore in memory, sorted by score descending then seq ascending (SR-FW-10).
class HighScoreStore {
  public:
    /// One game's rows, ranks 1..count in order (SR-FW-10).
    struct GameTable {
        std::array<HighScoreRow, HISCORE_SLOTS> rows{};
        std::size_t count = 0;
    };

    /// Replaces the table with the contents of `path`. A missing or unreadable file gives an
    /// empty table (SR-FW-08); lines that fail validation are dropped (SC-04). Ranks are
    /// recomputed from score and seq (SR-FW-10).
    void load(FileSystem& fs, std::string_view path = HISCORE_FILE);

    /// Writes the table to `path` + ".tmp", calls fsync(), then rename() over `path`
    /// (SR-FW-09). Each line holds only game_id,rank,initials,score,seq (SC-03).
    SaveStatus save(FileSystem& fs, std::string_view path = HISCORE_FILE) const;

    /// True if `score` would enter `gameId`'s top 10 (SR-FW-10, used by SR-UI-08). An equal
    /// score ranks below older entries, so it qualifies only while a slot is free.
    bool qualifies(std::uint8_t gameId, std::uint32_t score) const noexcept;

    /// Adds an entry with the next seq and returns its rank, or std::nullopt if it does not
    /// qualify, is invalid, or every seq is used (SR-FW-10, SC-04). An 11th entry drops the
    /// lowest.
    std::optional<std::uint8_t> insert(std::uint8_t gameId,
                                       const std::array<char, INITIALS_LEN>& initials,
                                       std::uint32_t score) noexcept;

    /// The rows of `gameId`; an empty table for an unknown game (SR-FW-10, SR-UI-12).
    const GameTable& table(std::uint8_t gameId) const noexcept;

    /// Parses and validates one CSV line `game_id,rank,initials,score,seq` (SC-04).
    static std::optional<HighScoreRow> parseLine(std::string_view line) noexcept;

  private:
    void add(const HighScoreRow& row) noexcept;

    std::array<GameTable, kHighScoreGameCount> tables_{};
    /// Next seq to assign; above UINT32 max once every seq is used, so no seq repeats
    /// (SR-FW-10).
    std::uint64_t nextSeq_ = 0;
};

}  // namespace arcade
