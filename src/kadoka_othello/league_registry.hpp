#pragma once

#include <cstddef>
#include <iosfwd>
#include <string>
#include <string_view>
#include <vector>

#include "kadoka_othello/league.hpp"

namespace kadoka::othello {

inline constexpr const char* kLeagueRegistryFormat =
    "kadoka.league_registry.v1";

enum class LeagueParticipantRole {
    Standard,
    Champion,
    Candidate,
    HallOfFame,
};

struct LeagueRegistryEntry {
    LeagueParticipant participant;
    LeagueRating rating;
    LeagueParticipantRole role{LeagueParticipantRole::Standard};

    // Optional human-readable checkpoint/generation identifier. Participant
    // identity still comes from package/config content fingerprinting.
    std::string checkpoint_id;
};

class LeagueRegistry {
public:
    void register_entry(LeagueRegistryEntry entry);

    [[nodiscard]] const LeagueRegistryEntry* find(
        const std::string& participant_id) const noexcept;
    [[nodiscard]] LeagueRegistryEntry* find(
        const std::string& participant_id) noexcept;

    [[nodiscard]] const std::vector<LeagueRegistryEntry>& entries()
        const noexcept;
    [[nodiscard]] std::vector<LeagueRegistryEntry>& entries() noexcept;

    void apply_run_result(const LeagueRunResult& result);

private:
    std::vector<LeagueRegistryEntry> entries_;
};

[[nodiscard]] const char* league_participant_role_name(
    LeagueParticipantRole role) noexcept;
[[nodiscard]] LeagueParticipantRole parse_league_participant_role(
    std::string_view value);

[[nodiscard]] std::string league_registry_entry_to_json(
    const LeagueRegistryEntry& entry);
[[nodiscard]] LeagueRegistryEntry parse_league_registry_entry_json(
    std::string_view json);

void write_league_registry_jsonl(
    const LeagueRegistry& registry,
    std::ostream& output);

[[nodiscard]] LeagueRegistry read_league_registry_jsonl(
    std::istream& input);

}  // namespace kadoka::othello
