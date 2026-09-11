#pragma once

#include <vector>

#include "kadoka_othello/board.hpp"

namespace kadoka::othello::rules {

[[nodiscard]] std::vector<Position> flips_for_move(const Board& board, Position position, Player player);
[[nodiscard]] bool is_legal_move(const Board& board, Position position, Player player);
[[nodiscard]] std::vector<Position> legal_moves(const Board& board, Player player);
[[nodiscard]] bool has_legal_move(const Board& board, Player player);
bool apply_move(Board& board, Position position, Player player);

}  // namespace kadoka::othello::rules
