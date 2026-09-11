#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>

#include "kadoka_othello/state.hpp"

namespace kadoka::othello {

struct HeadlessConfig {
    std::size_t board_size{8};
    std::size_t games{1};
    std::uint64_t seed{0};
    bool write_json_lines{true};
};

struct HeadlessSummary {
    std::size_t games{};
    std::size_t black_wins{};
    std::size_t white_wins{};
    std::size_t draws{};
};

[[nodiscard]] HeadlessSummary run_random_games(
    const HeadlessConfig& config,
    std::ostream* dataset_output = nullptr);

}  // namespace kadoka::othello
