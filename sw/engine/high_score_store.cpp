#include "engine/high_score_store.hpp"

#include <limits>

namespace arcade {
namespace {

/// Parses an unsigned decimal field with no sign or spaces; std::nullopt if empty, not all
/// digits, or above `max` (SC-04).
std::optional<std::uint32_t> parseUnsigned(std::string_view text, std::uint32_t max) noexcept {
    if (text.empty()) {
        return std::nullopt;
    }
    std::uint64_t value = 0;
    for (const char c : text) {
        if (c < '0' || c > '9') {
            return std::nullopt;
        }
        value = value * 10 + static_cast<std::uint64_t>(c - '0');
        if (value > max) {
            return std::nullopt;
        }
    }
    return static_cast<std::uint32_t>(value);
}

/// SC-04: initials are exactly three letters A-Z.
bool validInitials(const std::array<char, INITIALS_LEN>& initials) noexcept {
    for (const char c : initials) {
        if (c < 'A' || c > 'Z') {
            return false;
        }
    }
    return true;
}

/// SR-FW-10: higher score first; on a tie the older entry (lower seq) first.
bool ranksAbove(const HighScoreRow& a, const HighScoreRow& b) noexcept {
    return a.score != b.score ? a.score > b.score : a.seq < b.seq;
}

}  // namespace

std::optional<HighScoreRow> HighScoreStore::parseLine(std::string_view line) noexcept {
    if (!line.empty() && line.back() == '\r') {
        line.remove_suffix(1);  // Accept CRLF line ends.
    }
    std::array<std::string_view, kHighScoreColumnCount> fields{};
    std::size_t n = 0;
    while (true) {
        const std::size_t pos = line.find(kHighScoreSeparator);
        if (n == kHighScoreColumnCount) {
            return std::nullopt;  // Too many columns.
        }
        fields[n++] = line.substr(0, pos);
        if (pos == std::string_view::npos) {
            break;
        }
        line.remove_prefix(pos + 1);
    }
    if (n != kHighScoreColumnCount) {
        return std::nullopt;
    }

    const auto col = [&fields](HighScoreColumn c) { return fields[static_cast<std::size_t>(c)]; };
    const auto gameId = parseUnsigned(col(HighScoreColumn::GameId), kHighScoreGameCount - 1);
    const auto rank = parseUnsigned(col(HighScoreColumn::Rank), kRankMax);     // SC-04: 1-10
    const auto score = parseUnsigned(col(HighScoreColumn::Score), SCORE_MAX);  // SC-04
    const auto seq =
        parseUnsigned(col(HighScoreColumn::Seq), std::numeric_limits<std::uint32_t>::max());
    const std::string_view initialsText = col(HighScoreColumn::Initials);
    if (!gameId || !rank || *rank < kRankMin || !score || !seq ||
        initialsText.size() != INITIALS_LEN) {
        return std::nullopt;
    }

    HighScoreRow row{};
    row.game_id = static_cast<std::uint8_t>(*gameId);
    row.rank = static_cast<std::uint8_t>(*rank);
    for (std::size_t i = 0; i < INITIALS_LEN; ++i) {
        row.initials[i] = initialsText[i];
    }
    if (!validInitials(row.initials)) {
        return std::nullopt;
    }
    row.score = *score;
    row.seq = *seq;
    return row;
}

void HighScoreStore::load(FileSystem& fs, std::string_view path) {
    tables_ = {};
    nextSeq_ = 0;
    // SR-FW-08: a missing or unreadable file gives an empty table.
    const std::optional<std::string> text = fs.readAll(std::string(path));
    if (!text) {
        return;
    }
    std::string_view rest = *text;
    while (!rest.empty()) {
        const std::size_t end = rest.find('\n');
        const std::string_view line = rest.substr(0, end);
        rest = end == std::string_view::npos ? std::string_view{} : rest.substr(end + 1);
        // SC-04: drop any line that fails validation, keep the rest.
        if (const auto row = parseLine(line)) {
            add(*row);
            if (row->seq >= nextSeq_) {
                nextSeq_ = std::uint64_t{row->seq} + 1;
            }
        }
    }
}

void HighScoreStore::add(const HighScoreRow& row) noexcept {
    // SR-FW-10: keep the top HISCORE_SLOTS sorted; an extra entry drops the lowest.
    GameTable& t = tables_[row.game_id];
    std::size_t pos = 0;
    while (pos < t.count && ranksAbove(t.rows[pos], row)) {
        ++pos;
    }
    if (pos == HISCORE_SLOTS) {
        return;
    }
    const std::size_t last = t.count < HISCORE_SLOTS ? t.count : HISCORE_SLOTS - 1;
    for (std::size_t i = last; i > pos; --i) {
        t.rows[i] = t.rows[i - 1];
    }
    t.rows[pos] = row;
    if (t.count < HISCORE_SLOTS) {
        ++t.count;
    }
    for (std::size_t i = 0; i < t.count; ++i) {
        t.rows[i].rank = static_cast<std::uint8_t>(kRankMin + i);
    }
}

bool HighScoreStore::qualifies(std::uint8_t gameId, std::uint32_t score) const noexcept {
    if (gameId >= kHighScoreGameCount || score > SCORE_MAX) {
        return false;
    }
    const GameTable& t = tables_[gameId];
    // SR-FW-10: a new entry is the newest, so it must beat the lowest score strictly.
    return t.count < HISCORE_SLOTS || score > t.rows[t.count - 1].score;
}

std::optional<std::uint8_t> HighScoreStore::insert(std::uint8_t gameId,
                                                   const std::array<char, INITIALS_LEN>& initials,
                                                   std::uint32_t score) noexcept {
    // SR-FW-10: refuse once every seq is used, so two entries never share one and the
    // older entry always wins a tie.
    if (nextSeq_ > std::numeric_limits<std::uint32_t>::max() || !qualifies(gameId, score) ||
        !validInitials(initials)) {
        return std::nullopt;
    }
    HighScoreRow row{gameId, kRankMin, initials, score, static_cast<std::uint32_t>(nextSeq_)};
    ++nextSeq_;
    add(row);
    const GameTable& t = tables_[gameId];
    for (std::size_t i = 0; i < t.count; ++i) {
        if (t.rows[i].seq == row.seq && t.rows[i].score == score) {
            return t.rows[i].rank;
        }
    }
    return std::nullopt;
}

const HighScoreStore::GameTable& HighScoreStore::table(std::uint8_t gameId) const noexcept {
    static const GameTable kEmpty{};
    return gameId < kHighScoreGameCount ? tables_[gameId] : kEmpty;
}

SaveStatus HighScoreStore::save(FileSystem& fs, std::string_view path) const {
    // SC-03: each line holds only game_id,rank,initials,score,seq.
    std::string text;
    for (const GameTable& t : tables_) {
        for (std::size_t i = 0; i < t.count; ++i) {
            const HighScoreRow& r = t.rows[i];
            text += std::to_string(r.game_id);
            text += kHighScoreSeparator;
            text += std::to_string(r.rank);
            text += kHighScoreSeparator;
            text.append(r.initials.data(), INITIALS_LEN);
            text += kHighScoreSeparator;
            text += std::to_string(r.score);
            text += kHighScoreSeparator;
            text += std::to_string(r.seq);
            text += '\n';
        }
    }

    // SR-FW-09: write a temporary file, fsync() it, then rename() it over the old file.
    const std::string target(path);
    const std::string temp = target + ".tmp";
    const int fd = fs.openForWrite(temp);
    if (fd < 0) {
        return SaveStatus::OpenFailed;
    }
    if (!fs.writeAll(fd, text)) {
        fs.close(fd);
        return SaveStatus::WriteFailed;
    }
    if (!fs.sync(fd)) {
        fs.close(fd);
        return SaveStatus::SyncFailed;
    }
    if (!fs.close(fd)) {
        return SaveStatus::CloseFailed;
    }
    if (!fs.rename(temp, target)) {
        return SaveStatus::RenameFailed;
    }
    return SaveStatus::Ok;
}

}  // namespace arcade
