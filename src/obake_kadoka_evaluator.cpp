#include "kadoka_othello/obake_kadoka_evaluator.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>

namespace kadoka::othello {
namespace {

constexpr std::array<std::pair<int, int>, 8> kDirections{{
    {-1, -1}, {-1, 0}, {-1, 1},
    {0, -1},           {0, 1},
    {1, -1},  {1, 0},  {1, 1},
}};

bool in_bounds(const Board& board, int row, int col) noexcept {
    return row >= 0 && col >= 0 &&
           row < static_cast<int>(board.size()) &&
           col < static_cast<int>(board.size());
}

int count_line_interest(const Board& board, Position move) {
    int interest = 0;
    for (const auto [dr, dc] : kDirections) {
        int row = static_cast<int>(move.row) + dr;
        int col = static_cast<int>(move.col) + dc;
        if (!in_bounds(board, row, col)) continue;

        const Cell first = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
        if (first == Cell::Empty) continue;

        int run = 0;
        while (in_bounds(board, row, col)) {
            const Cell current = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
            if (current != first) break;
            ++run;
            row += dr;
            col += dc;
        }

        if (run >= 1 && in_bounds(board, row, col)) {
            const Cell terminal = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
            if (terminal != Cell::Empty && terminal != first) interest += std::min(run, 3);
        }
    }
    return interest;
}

int count_color_transitions(const Board& board, Position move) {
    int transitions = 0;
    for (const auto [dr, dc] : kDirections) {
        int row = static_cast<int>(move.row) + dr;
        int col = static_cast<int>(move.col) + dc;
        Cell previous = Cell::Empty;
        int steps = 0;
        while (in_bounds(board, row, col) && steps < 4) {
            const Cell current = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
            if (current == Cell::Empty) break;
            if (previous != Cell::Empty && current != previous) ++transitions;
            previous = current;
            row += dr;
            col += dc;
            ++steps;
        }
    }
    return transitions;
}

}  // namespace

ObakeKadokaEvaluator::ObakeKadokaEvaluator(ObakeKadokaEvaluatorWeights weights)
    : weights_(weights) {}

const ObakeKadokaEvaluatorWeights& ObakeKadokaEvaluator::weights() const noexcept {
    return weights_;
}

void ObakeKadokaEvaluator::set_weights(ObakeKadokaEvaluatorWeights weights) noexcept {
    weights_ = weights;
}

ObakeKadokaEvaluation ObakeKadokaEvaluator::evaluate(
    const Board& board,
    Position move,
    std::size_t empty_cells) const {
    ObakeKadokaFeatureVector features;
    bool touches_black = false;
    bool touches_white = false;

    for (const auto [dr, dc] : kDirections) {
        const int row = static_cast<int>(move.row) + dr;
        const int col = static_cast<int>(move.col) + dc;
        if (!in_bounds(board, row, col)) continue;

        const Cell cell = board.at({static_cast<std::size_t>(row), static_cast<std::size_t>(col)});
        if (cell == Cell::Empty) {
            features.empty_neighbors += 1.0;
        } else {
            features.occupied_neighbors += 1.0;
            touches_black = touches_black || cell == Cell::Black;
            touches_white = touches_white || cell == Cell::White;
        }
    }

    features.mixed_color = touches_black && touches_white ? 1.0 : 0.0;
    features.color_transitions = static_cast<double>(count_color_transitions(board, move));
    features.line_interest = static_cast<double>(count_line_interest(board, move));
    features.local_density = features.occupied_neighbors * features.occupied_neighbors * 0.1;

    const double empty_ratio = static_cast<double>(empty_cells) /
                               static_cast<double>(board.size() * board.size());
    if (empty_ratio > 0.55) {
        const double center = (static_cast<double>(board.size()) - 1.0) * 0.5;
        const double distance =
            std::abs(static_cast<double>(move.row) - center) +
            std::abs(static_cast<double>(move.col) - center);
        features.early_center = std::max(0.0, center * 2.0 - distance);
    }

    const double score =
        features.occupied_neighbors * weights_.occupied_neighbors +
        features.empty_neighbors * weights_.empty_neighbors +
        features.mixed_color * weights_.mixed_color +
        features.color_transitions * weights_.color_transitions +
        features.line_interest * weights_.line_interest +
        features.local_density * weights_.local_density +
        features.early_center * weights_.early_center;

    return ObakeKadokaEvaluation{features, score};
}

}  // namespace kadoka::othello
