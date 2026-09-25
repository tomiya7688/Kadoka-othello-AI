#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <string>
#include <vector>

#include "kadoka_othello/ai.hpp"
#include "kadoka_othello/state.hpp"

namespace kadoka::othello {

struct HeadlessConfig {
    std::size_t board_size{8};
    std::size_t games{1};
    std::uint64_t seed{0};
    std::size_t max_invalid_attempts_per_turn{1024};
    bool write_json_lines{true};
    bool collect_metrics{false};

    // Optional explicit ID for a single-game Game Record. Empty means the
    // recorder generates a ULID.
    std::string record_game_id;
};

struct HeadlessAIMetrics {
    std::size_t calls{};
    double total_think_us{};
    double max_think_us{};
    std::uint64_t total_nodes{};
    std::uint64_t total_simulations{};
    std::size_t max_depth{};
    double total_search_effort{};
    std::size_t node_reports{};
    std::size_t simulation_reports{};
    std::size_t depth_reports{};
    std::size_t search_effort_reports{};
};

struct HeadlessSummary {
    std::size_t games{};
    std::size_t black_wins{};
    std::size_t white_wins{};
    std::size_t draws{};
    std::size_t total_black_discs{};
    std::size_t total_white_discs{};
    std::int64_t total_disc_difference{};
    std::size_t turns{};
    std::size_t ai_calls{};
    std::size_t invalid_move_attempts{};
    std::size_t turns_with_invalid_attempts{};
    std::size_t max_invalid_attempts_in_turn{};
    double total_ai_think_us{};
    double max_ai_think_us{};

    std::uint64_t total_nodes{};
    std::uint64_t total_simulations{};
    std::size_t max_depth{};
    double total_search_effort{};
    std::size_t node_reports{};
    std::size_t simulation_reports{};
    std::size_t depth_reports{};
    std::size_t search_effort_reports{};

    HeadlessAIMetrics black_ai_metrics;
    HeadlessAIMetrics white_ai_metrics;

    std::vector<std::size_t> invalid_attempt_histogram;
};

[[nodiscard]] HeadlessSummary run_games(
    const HeadlessConfig& config,
    AIPackage black,
    AIPackage white,
    std::ostream* dataset_output = nullptr,
    std::ostream* board_state_output = nullptr,
    std::ostream* game_aux_output = nullptr);

[[nodiscard]] HeadlessSummary run_random_games(
    const HeadlessConfig& config,
    std::ostream* dataset_output = nullptr,
    std::ostream* board_state_output = nullptr,
    std::ostream* game_aux_output = nullptr);

}  // namespace kadoka::othello
