#include "kadoka_othello/league_registry.hpp"

#include <cctype>
#include <iomanip>
#include <istream>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace kadoka::othello {
namespace {

std::string escape_json(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += ch; break;
        }
    }
    return escaped;
}

std::size_t find_value_start(
    std::string_view json,
    std::string_view key) {
    const std::string token = "\"" + std::string(key) + "\"";
    std::size_t search_from = 0;
    while (true) {
        const std::size_t key_pos = json.find(token, search_from);
        if (key_pos == std::string_view::npos) {
            throw std::invalid_argument(
                "league registry missing field: " +
                std::string(key));
        }

        std::size_t colon = key_pos + token.size();
        while (colon < json.size() &&
               std::isspace(static_cast<unsigned char>(json[colon]))) {
            ++colon;
        }
        if (colon < json.size() && json[colon] == ':') {
            std::size_t pos = colon + 1;
            while (pos < json.size() &&
                   std::isspace(static_cast<unsigned char>(json[pos]))) {
                ++pos;
            }
            return pos;
        }
        search_from = key_pos + token.size();
    }
}

std::string parse_string_at(
    std::string_view json,
    std::size_t pos) {
    if (pos >= json.size() || json[pos] != '"') {
        throw std::invalid_argument(
            "league registry expected string");
    }

    std::string result;
    for (++pos; pos < json.size(); ++pos) {
        const char ch = json[pos];
        if (ch == '"') return result;
        if (ch != '\\') {
            result += ch;
            continue;
        }
        if (++pos >= json.size()) {
            throw std::invalid_argument(
                "league registry unterminated escape");
        }
        switch (json[pos]) {
            case '\\': result += '\\'; break;
            case '"': result += '"'; break;
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            default:
                throw std::invalid_argument(
                    "league registry unsupported JSON escape");
        }
    }
    throw std::invalid_argument(
        "league registry unterminated string");
}

std::string read_string(
    std::string_view json,
    std::string_view key) {
    return parse_string_at(json, find_value_start(json, key));
}

std::uint64_t read_uint(
    std::string_view json,
    std::string_view key) {
    std::size_t pos = find_value_start(json, key);
    const std::size_t begin = pos;
    while (pos < json.size() &&
           std::isdigit(static_cast<unsigned char>(json[pos]))) {
        ++pos;
    }
    if (pos == begin) {
        throw std::invalid_argument(
            "league registry expected unsigned integer: " +
            std::string(key));
    }
    return std::stoull(std::string(json.substr(begin, pos - begin)));
}

double read_double(
    std::string_view json,
    std::string_view key) {
    std::size_t pos = find_value_start(json, key);
    const std::size_t begin = pos;
    while (pos < json.size()) {
        const char ch = json[pos];
        if (!(std::isdigit(static_cast<unsigned char>(ch)) ||
              ch == '-' || ch == '+' || ch == '.' ||
              ch == 'e' || ch == 'E')) {
            break;
        }
        ++pos;
    }
    if (pos == begin) {
        throw std::invalid_argument(
            "league registry expected number: " +
            std::string(key));
    }
    return std::stod(std::string(json.substr(begin, pos - begin)));
}

void validate_entry(const LeagueRegistryEntry& entry) {
    if (entry.participant.participant_id.empty()) {
        throw std::invalid_argument(
            "league registry participant_id must not be empty");
    }
    if (entry.participant.config.manifest_path.empty()) {
        throw std::invalid_argument(
            "league registry manifest_path must not be empty");
    }
    if (entry.participant.config.board_size != 6 &&
        entry.participant.config.board_size != 8 &&
        entry.participant.config.board_size != 10) {
        throw std::invalid_argument(
            "league registry board_size must be 6, 8 or 10");
    }
    if (entry.rating.deviation <= 0.0) {
        throw std::invalid_argument(
            "league registry rating deviation must be positive");
    }
    if (entry.rating.wins +
            entry.rating.losses +
            entry.rating.draws !=
        entry.rating.games) {
        throw std::invalid_argument(
            "league registry W/L/D count must equal games");
    }
}

}  // namespace

const char* league_participant_role_name(
    LeagueParticipantRole role) noexcept {
    switch (role) {
        case LeagueParticipantRole::Standard:
            return "standard";
        case LeagueParticipantRole::Champion:
            return "champion";
        case LeagueParticipantRole::Candidate:
            return "candidate";
        case LeagueParticipantRole::HallOfFame:
            return "hall_of_fame";
    }
    return "unknown";
}

LeagueParticipantRole parse_league_participant_role(
    std::string_view value) {
    if (value == "standard") {
        return LeagueParticipantRole::Standard;
    }
    if (value == "champion") {
        return LeagueParticipantRole::Champion;
    }
    if (value == "candidate") {
        return LeagueParticipantRole::Candidate;
    }
    if (value == "hall_of_fame") {
        return LeagueParticipantRole::HallOfFame;
    }
    throw std::invalid_argument(
        "unknown league participant role: " +
        std::string(value));
}

void LeagueRegistry::register_entry(
    LeagueRegistryEntry entry) {
    validate_entry(entry);
    if (find(entry.participant.participant_id) != nullptr) {
        throw std::invalid_argument(
            "duplicate league participant_id: " +
            entry.participant.participant_id);
    }
    entries_.push_back(std::move(entry));
}

const LeagueRegistryEntry* LeagueRegistry::find(
    const std::string& participant_id) const noexcept {
    for (const auto& entry : entries_) {
        if (entry.participant.participant_id ==
            participant_id) {
            return &entry;
        }
    }
    return nullptr;
}

LeagueRegistryEntry* LeagueRegistry::find(
    const std::string& participant_id) noexcept {
    for (auto& entry : entries_) {
        if (entry.participant.participant_id ==
            participant_id) {
            return &entry;
        }
    }
    return nullptr;
}

const std::vector<LeagueRegistryEntry>&
LeagueRegistry::entries() const noexcept {
    return entries_;
}

std::vector<LeagueRegistryEntry>&
LeagueRegistry::entries() noexcept {
    return entries_;
}

void LeagueRegistry::apply_run_result(
    const LeagueRunResult& result) {
    for (const auto& table_entry : result.table) {
        LeagueRegistryEntry* entry =
            find(table_entry.participant.participant_id);
        if (entry == nullptr) {
            throw std::invalid_argument(
                "league result contains participant missing from registry: " +
                table_entry.participant.participant_id);
        }
        entry->rating = table_entry.rating;
    }
}

std::string league_registry_entry_to_json(
    const LeagueRegistryEntry& entry) {
    validate_entry(entry);

    std::ostringstream out;
    out << "{\"format\":\"" << kLeagueRegistryFormat << "\""
        << ",\"participant_id\":\""
        << escape_json(entry.participant.participant_id) << "\""
        << ",\"role\":\""
        << league_participant_role_name(entry.role) << "\""
        << ",\"checkpoint_id\":\""
        << escape_json(entry.checkpoint_id) << "\""
        << ",\"manifest_path\":\""
        << escape_json(entry.participant.config.manifest_path) << "\""
        << ",\"board_size\":"
        << entry.participant.config.board_size
        << ",\"seed\":"
        << entry.participant.config.seed
        << ",\"config_hash\":\""
        << escape_json(entry.participant.config.config_hash) << "\""
        << ",\"rating\":" << std::setprecision(17)
        << entry.rating.rating
        << ",\"deviation\":" << std::setprecision(17)
        << entry.rating.deviation
        << ",\"games\":" << entry.rating.games
        << ",\"wins\":" << entry.rating.wins
        << ",\"losses\":" << entry.rating.losses
        << ",\"draws\":" << entry.rating.draws
        << '}';
    return out.str();
}

LeagueRegistryEntry parse_league_registry_entry_json(
    std::string_view json) {
    if (read_string(json, "format") !=
        kLeagueRegistryFormat) {
        throw std::invalid_argument(
            "unsupported league registry format");
    }

    LeagueParticipantConfig config;
    config.manifest_path =
        read_string(json, "manifest_path");
    config.board_size = static_cast<std::size_t>(
        read_uint(json, "board_size"));
    config.seed = read_uint(json, "seed");
    config.config_hash =
        read_string(json, "config_hash");

    LeagueRegistryEntry entry;
    entry.participant =
        load_league_participant(std::move(config));
    const std::string stored_participant_id =
        read_string(json, "participant_id");
    if (stored_participant_id !=
        entry.participant.participant_id) {
        throw std::invalid_argument(
            "league registry participant fingerprint no longer matches package/config: " +
            stored_participant_id);
    }

    entry.role = parse_league_participant_role(
        read_string(json, "role"));
    entry.checkpoint_id =
        read_string(json, "checkpoint_id");
    entry.rating.rating = read_double(json, "rating");
    entry.rating.deviation =
        read_double(json, "deviation");
    entry.rating.games = static_cast<std::size_t>(
        read_uint(json, "games"));
    entry.rating.wins = static_cast<std::size_t>(
        read_uint(json, "wins"));
    entry.rating.losses = static_cast<std::size_t>(
        read_uint(json, "losses"));
    entry.rating.draws = static_cast<std::size_t>(
        read_uint(json, "draws"));

    validate_entry(entry);
    return entry;
}

void write_league_registry_jsonl(
    const LeagueRegistry& registry,
    std::ostream& output) {
    for (const auto& entry : registry.entries()) {
        output << league_registry_entry_to_json(entry)
               << '\n';
    }
}

LeagueRegistry read_league_registry_jsonl(
    std::istream& input) {
    LeagueRegistry registry;
    std::string line;
    while (std::getline(input, line)) {
        if (line.empty()) continue;
        registry.register_entry(
            parse_league_registry_entry_json(line));
    }
    return registry;
}

}  // namespace kadoka::othello
