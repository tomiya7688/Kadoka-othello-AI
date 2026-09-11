#pragma once

#include <optional>
#include <string>
#include <vector>

#include "kadoka_othello/game.hpp"

namespace kadoka::othello {

struct GameSnapshot {
    std::size_t board_size{};
    Player current_player{Player::Black};
    GameStatus status{GameStatus::Playing};
    std::vector<Cell> cells;
    std::vector<Position> legal_moves;
    std::vector<Move> history;
    std::optional<GameResult> result;
};

[[nodiscard]] GameSnapshot make_snapshot(const Game& game);
[[nodiscard]] std::string snapshot_to_json(const GameSnapshot& snapshot);

}  // namespace kadoka::othello
