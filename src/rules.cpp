#include "kadoka_othello/rules.hpp"

#include <array>
#include <utility>

namespace kadoka::othello::rules {
namespace {

constexpr std::array<std::pair<int, int>, 8> kDirections{{
    {-1, -1}, {-1, 0}, {-1, 1},
    {0, -1},           {0, 1},
    {1, -1},  {1, 0},  {1, 1},
}};

bool in_bounds_signed(const Board& board, int row, int col) noexcept {
    return row >= 0 && col >= 0 &&
           row < static_cast<int>(board.size()) &&
           col < static_cast<int>(board.size());
}

}  // namespace

std::vector<Position> flips_for_move(
    const Board& board,
    Position position,
    Player player) {
    std::vector<Position> flips;

    if (!board.in_bounds(position) || board.at(position) != Cell::Empty) {
        return flips;
    }

    const Cell own = to_cell(player);
    const Cell enemy = to_cell(opponent(player));

    for (const auto [dr, dc] : kDirections) {
        std::vector<Position> line;
        int row = static_cast<int>(position.row) + dr;
        int col = static_cast<int>(position.col) + dc;

        while (in_bounds_signed(board, row, col)) {
            const Position current{
                static_cast<std::size_t>(row),
                static_cast<std::size_t>(col),
            };
            const Cell cell = board.at(current);

            if (cell == enemy) {
                line.push_back(current);
                row += dr;
                col += dc;
                continue;
            }

            if (cell == own && !line.empty()) {
                flips.insert(flips.end(), line.begin(), line.end());
            }
            break;
        }
    }

    return flips;
}

bool is_legal_move(
    const Board& board,
    Position position,
    Player player) {
    return !flips_for_move(board, position, player).empty();
}

std::vector<Position> legal_moves(
    const Board& board,
    Player player) {
    std::vector<Position> moves;

    for (std::size_t row = 0; row < board.size(); ++row) {
        for (std::size_t col = 0; col < board.size(); ++col) {
            const Position position{row, col};
            if (is_legal_move(board, position, player)) {
                moves.push_back(position);
            }
        }
    }

    return moves;
}

bool has_legal_move(
    const Board& board,
    Player player) {
    for (std::size_t row = 0; row < board.size(); ++row) {
        for (std::size_t col = 0; col < board.size(); ++col) {
            if (is_legal_move(board, {row, col}, player)) {
                return true;
            }
        }
    }
    return false;
}

bool apply_move(
    Board& board,
    Position position,
    Player player) {
    const auto flips = flips_for_move(board, position, player);
    if (flips.empty()) {
        return false;
    }

    const Cell cell = to_cell(player);
    board.set(position, cell);
    for (const Position flipped : flips) {
        board.set(flipped, cell);
    }

    return true;
}

}  // namespace kadoka::othello::rules
