#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "kadoka_othello/headless.hpp"
#include "kadoka_othello/package.hpp"

namespace kadoka::othello {

struct LeagueParticipantConfig {
    std::string manifest_path;
    std::size_t board_size{8};
    std::uint64_t seed{1};

    // Caller-owned hash/string for settings that are not represented by the
    // current manifest/model files (time limits, search overrides, etc.).
    std::string config_hash;
};

struct LeagueParticipant {
    LeagueParticipantConfig config;
    AIPackageManifest manifest;
    std::string participant_id;
};

struct LeagueRating {
    double rating{1500.0};
    double deviation{350.0};
    std::size_t games{};
    std::size_t wins{};
    std::size_t losses{};
    std::size_t draws{};
};

enum class LeagueGameOutcome {
    BlackWin,
    WhiteWin,
    Draw,
};

struct LeagueGameRecord {
    // ULID shared with Game Record v1 when BoardState/GameAux output is enabled.
    std::string game_id;

    // Deterministic key derived from participants/config for reproduction and
    // duplicate-run analysis. It is not the Game Record primary key.
    std::string reproducibility_key;

    std::string black_participant_id;
    std::string white_participant_id;
    std::size_t board_size{};
    std::uint64_t black_seed{};
    std::uint64_t white_seed{};
    LeagueGameOutcome outcome{LeagueGameOutcome::Draw};
    double elapsed_us{};
    HeadlessSummary metrics;
    double black_rating_before{};
    double white_rating_before{};
    double black_deviation_before{};
    double white_deviation_before{};
    double black_rating_after{};
    double white_rating_after{};
    double black_deviation_after{};
    double white_deviation_after{};
};

struct LeagueRunConfig {
    // One means each scheduled pair plays both colors once.
    std::size_t games_per_color{1};
    std::uint64_t seed{1};
    std::size_t max_invalid_attempts_per_turn{1024};
    bool collect_metrics{true};
};

struct LeagueTableEntry {
    LeagueParticipant participant;
    LeagueRating rating;
};

struct LeagueRunResult {
    std::vector<LeagueGameRecord> games;
    std::vector<LeagueTableEntry> table;
};

struct LeaguePairing {
    std::size_t first{};
    std::size_t second{};
};

struct LeagueRunOutputs {
    std::ostream* game_log{nullptr};

    // Transitional compatibility stream. New Dataset work should prefer the
    // Game Record v1 streams below.
    std::ostream* position_dataset{nullptr};

    std::ostream* board_state_output{nullptr};
    std::ostream* game_aux_output{nullptr};
};

[[nodiscard]] LeagueParticipant load_league_participant(
    LeagueParticipantConfig config);

[[nodiscard]] LeagueRunResult run_league_schedule(
    const std::vector<LeagueParticipant>& participants,
    const std::vector<LeagueRating>& initial_ratings,
    const std::vector<LeaguePairing>& pairings,
    const LeagueRunConfig& config,
    const LeagueRunOutputs& outputs = {});

[[nodiscard]] LeagueRunResult run_round_robin_league(
    const std::vector<LeagueParticipant>& participants,
    const LeagueRunConfig& config,
    std::ostream* game_log = nullptr,
    std::ostream* position_dataset = nullptr);

[[nodiscard]] const char* league_outcome_name(
    LeagueGameOutcome outcome) noexcept;

}  // namespace kadoka::othello
