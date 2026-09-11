#pragma once

#include <cstddef>
#include <cstdint>

namespace kadoka::othello {

enum class Cell : std::uint8_t {
    Empty = 0,
    Black = 1,
    White = 2,
};

enum class Player : std::uint8_t {
    Black = 1,
    White = 2,
};

struct Position {
    std::size_t row{};
    std::size_t col{};

    friend constexpr bool operator==(const Position& lhs, const Position& rhs) noexcept {
        return lhs.row == rhs.row && lhs.col == rhs.col;
    }

    friend constexpr bool operator!=(const Position& lhs, const Position& rhs) noexcept {
        return !(lhs == rhs);
    }
};

struct Move {
    Player player{Player::Black};
    Position position{};
};

constexpr Cell to_cell(Player player) noexcept {
    return player == Player::Black ? Cell::Black : Cell::White;
}

constexpr Player opponent(Player player) noexcept {
    return player == Player::Black ? Player::White : Player::Black;
}

}  // namespace kadoka::othello
