#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "kadoka_othello/board.hpp"

namespace kadoka::othello {

inline constexpr const char* kCoreStateFormat = "kadoka.core_state.v1";

struct CoreTimeState {
    std::optional<std::uint64_t> black_remaining_ms;
    std::optional<std::uint64_t> white_remaining_ms;
    std::optional<std::uint64_t> move_limit_ms;
};

struct CoreStateView {
    const Board* board{nullptr};
    Player side_to_move{Player::Black};
    CoreTimeState time;
};

struct CoreState {
    std::size_t board_size{};
    std::vector<Cell> cells;
    Player side_to_move{Player::Black};
    CoreTimeState time;
};

[[nodiscard]] CoreState make_core_state(const CoreStateView& view);
[[nodiscard]] Board board_from_core_state(const CoreState& state);
[[nodiscard]] std::string core_state_to_json(const CoreStateView& view);
[[nodiscard]] std::string core_state_to_json(const CoreState& state);
[[nodiscard]] CoreState parse_core_state_json(std::string_view json);

}  // namespace kadoka::othello
