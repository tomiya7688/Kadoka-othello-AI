#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "kadoka_othello/league.hpp"
#include "kadoka_othello/league_registry.hpp"

namespace kadoka::othello {

struct LeagueRegistryPairing {
    std::size_t first_entry{};
    std::size_t second_entry{};
};

[[nodiscard]] std::vector<LeagueRegistryPairing>
schedule_round_robin(
    const LeagueRegistry& registry,
    std::size_t board_size);

[[nodiscard]] std::vector<LeagueRegistryPairing>
schedule_random_matches(
    const LeagueRegistry& registry,
    std::size_t board_size,
    std::size_t pair_count,
    std::uint64_t seed);

[[nodiscard]] std::vector<LeagueRegistryPairing>
schedule_rating_band(
    const LeagueRegistry& registry,
    std::size_t board_size,
    std::size_t pair_count,
    double max_rating_gap,
    std::uint64_t seed);

[[nodiscard]] std::vector<LeagueRegistryPairing>
schedule_candidate_champion(
    const LeagueRegistry& registry,
    std::size_t board_size);

[[nodiscard]] std::vector<LeagueRegistryPairing>
schedule_candidate_hall_of_fame(
    const LeagueRegistry& registry,
    std::size_t board_size);

[[nodiscard]] LeagueRunResult run_registry_schedule(
    LeagueRegistry& registry,
    std::size_t board_size,
    const std::vector<LeagueRegistryPairing>& pairings,
    const LeagueRunConfig& config,
    const LeagueRunOutputs& outputs = {});

}  // namespace kadoka::othello
