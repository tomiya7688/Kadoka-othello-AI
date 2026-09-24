#include "kadoka_othello/league_scheduler.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace kadoka::othello {
namespace {

std::vector<std::size_t> eligible_entries(
    const LeagueRegistry& registry,
    std::size_t board_size) {
    std::vector<std::size_t> indexes;
    const auto& entries = registry.entries();
    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (entries[i].participant.config.board_size == board_size) {
            indexes.push_back(i);
        }
    }
    return indexes;
}

std::vector<LeagueRegistryPairing> all_pairs(
    const std::vector<std::size_t>& indexes) {
    std::vector<LeagueRegistryPairing> pairs;
    for (std::size_t first = 0; first < indexes.size(); ++first) {
        for (std::size_t second = first + 1; second < indexes.size(); ++second) {
            pairs.push_back({indexes[first], indexes[second]});
        }
    }
    return pairs;
}

std::vector<LeagueRegistryPairing> repeat_shuffled_pairs(
    std::vector<LeagueRegistryPairing> pairs,
    std::size_t pair_count,
    std::uint64_t seed) {
    if (pair_count == 0) {
        throw std::invalid_argument("pair_count must be greater than zero");
    }
    if (pairs.empty()) {
        throw std::invalid_argument("no eligible league pairings");
    }

    std::mt19937_64 rng(seed == 0 ? 1 : seed);
    std::vector<LeagueRegistryPairing> schedule;
    schedule.reserve(pair_count);
    while (schedule.size() < pair_count) {
        std::shuffle(pairs.begin(), pairs.end(), rng);
        for (const LeagueRegistryPairing pair : pairs) {
            schedule.push_back(pair);
            if (schedule.size() == pair_count) break;
        }
    }
    return schedule;
}

void require_board_size(std::size_t board_size) {
    if (board_size != 6 && board_size != 8 && board_size != 10) {
        throw std::invalid_argument("league board size must be 6, 8 or 10");
    }
}

}  // namespace

std::vector<LeagueRegistryPairing> schedule_round_robin(
    const LeagueRegistry& registry,
    std::size_t board_size) {
    require_board_size(board_size);
    const std::vector<std::size_t> indexes =
        eligible_entries(registry, board_size);
    if (indexes.size() < 2) {
        throw std::invalid_argument(
            "round-robin requires at least two participants");
    }
    return all_pairs(indexes);
}

std::vector<LeagueRegistryPairing> schedule_random_matches(
    const LeagueRegistry& registry,
    std::size_t board_size,
    std::size_t pair_count,
    std::uint64_t seed) {
    require_board_size(board_size);
    const std::vector<std::size_t> indexes =
        eligible_entries(registry, board_size);
    if (indexes.size() < 2) {
        throw std::invalid_argument(
            "random scheduler requires at least two participants");
    }
    return repeat_shuffled_pairs(
        all_pairs(indexes),
        pair_count,
        seed);
}

std::vector<LeagueRegistryPairing> schedule_rating_band(
    const LeagueRegistry& registry,
    std::size_t board_size,
    std::size_t pair_count,
    double max_rating_gap,
    std::uint64_t seed) {
    require_board_size(board_size);
    if (!std::isfinite(max_rating_gap) || max_rating_gap < 0.0) {
        throw std::invalid_argument(
            "max_rating_gap must be finite and non-negative");
    }

    const auto& entries = registry.entries();
    const std::vector<std::size_t> indexes =
        eligible_entries(registry, board_size);
    std::vector<LeagueRegistryPairing> pairs;
    for (std::size_t first = 0; first < indexes.size(); ++first) {
        for (std::size_t second = first + 1;
             second < indexes.size();
             ++second) {
            const std::size_t lhs = indexes[first];
            const std::size_t rhs = indexes[second];
            if (std::abs(
                    entries[lhs].rating.rating -
                    entries[rhs].rating.rating) <= max_rating_gap) {
                pairs.push_back({lhs, rhs});
            }
        }
    }
    return repeat_shuffled_pairs(
        std::move(pairs),
        pair_count,
        seed);
}

std::vector<LeagueRegistryPairing> schedule_candidate_champion(
    const LeagueRegistry& registry,
    std::size_t board_size) {
    require_board_size(board_size);
    const auto& entries = registry.entries();
    std::vector<LeagueRegistryPairing> pairs;
    for (std::size_t candidate = 0;
         candidate < entries.size();
         ++candidate) {
        if (entries[candidate].participant.config.board_size != board_size ||
            entries[candidate].role != LeagueParticipantRole::Candidate) {
            continue;
        }
        for (std::size_t champion = 0;
             champion < entries.size();
             ++champion) {
            if (entries[champion].participant.config.board_size != board_size ||
                entries[champion].role != LeagueParticipantRole::Champion) {
                continue;
            }
            pairs.push_back({candidate, champion});
        }
    }
    if (pairs.empty()) {
        throw std::invalid_argument(
            "candidate/champion scheduler found no eligible pair");
    }
    return pairs;
}

std::vector<LeagueRegistryPairing> schedule_candidate_hall_of_fame(
    const LeagueRegistry& registry,
    std::size_t board_size) {
    require_board_size(board_size);
    const auto& entries = registry.entries();
    std::vector<LeagueRegistryPairing> pairs;
    for (std::size_t candidate = 0;
         candidate < entries.size();
         ++candidate) {
        if (entries[candidate].participant.config.board_size != board_size ||
            entries[candidate].role != LeagueParticipantRole::Candidate) {
            continue;
        }
        for (std::size_t historical = 0;
             historical < entries.size();
             ++historical) {
            if (entries[historical].participant.config.board_size != board_size ||
                entries[historical].role != LeagueParticipantRole::HallOfFame) {
                continue;
            }
            pairs.push_back({candidate, historical});
        }
    }
    if (pairs.empty()) {
        throw std::invalid_argument(
            "candidate/Hall-of-Fame scheduler found no eligible pair");
    }
    return pairs;
}

LeagueRunResult run_registry_schedule(
    LeagueRegistry& registry,
    std::size_t board_size,
    const std::vector<LeagueRegistryPairing>& pairings,
    const LeagueRunConfig& config,
    const LeagueRunOutputs& outputs) {
    require_board_size(board_size);
    if (pairings.empty()) {
        throw std::invalid_argument(
            "registry schedule must contain at least one pairing");
    }

    const auto& entries = registry.entries();
    std::vector<std::size_t> registry_indexes;
    std::unordered_map<std::size_t, std::size_t> local_index;

    auto add_entry = [&](std::size_t registry_index) {
        if (registry_index >= entries.size()) {
            throw std::invalid_argument(
                "registry schedule references invalid entry index");
        }
        if (entries[registry_index].participant.config.board_size !=
            board_size) {
            throw std::invalid_argument(
                "registry schedule mixes board sizes");
        }
        if (local_index.find(registry_index) != local_index.end()) return;
        const std::size_t local = registry_indexes.size();
        registry_indexes.push_back(registry_index);
        local_index.emplace(registry_index, local);
    };

    for (const LeagueRegistryPairing pairing : pairings) {
        if (pairing.first_entry == pairing.second_entry) {
            throw std::invalid_argument(
                "registry schedule cannot pair participant with itself");
        }
        add_entry(pairing.first_entry);
        add_entry(pairing.second_entry);
    }

    std::vector<LeagueParticipant> participants;
    std::vector<LeagueRating> ratings;
    participants.reserve(registry_indexes.size());
    ratings.reserve(registry_indexes.size());
    for (const std::size_t registry_index : registry_indexes) {
        participants.push_back(entries[registry_index].participant);
        ratings.push_back(entries[registry_index].rating);
    }

    std::vector<LeaguePairing> local_pairings;
    local_pairings.reserve(pairings.size());
    for (const LeagueRegistryPairing pairing : pairings) {
        local_pairings.push_back({
            local_index.at(pairing.first_entry),
            local_index.at(pairing.second_entry),
        });
    }

    LeagueRunResult result = run_league_schedule(
        participants,
        ratings,
        local_pairings,
        config,
        outputs);
    registry.apply_run_result(result);
    return result;
}

}  // namespace kadoka::othello
