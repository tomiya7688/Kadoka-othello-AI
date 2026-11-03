#include "kadoka_othello/evaluator_ai.hpp"
#include "kadoka_othello/model_descriptor.hpp"
#include "kadoka_othello/rules.hpp"
#include "test_support.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace kadoka::othello;

namespace {

double diagnostic_value(const AIInspection& inspection, const std::string& key) {
    for (const AIDiagnostic& diagnostic : inspection.diagnostics) {
        if (diagnostic.key == key) {
            std::size_t consumed = 0;
            const double value = std::stod(diagnostic.value, &consumed);
            if (consumed != diagnostic.value.size()) {
                throw std::runtime_error("invalid evaluator diagnostic value: " + key);
            }
            return value;
        }
    }
    throw std::runtime_error("missing evaluator diagnostic: " + key);
}

void verify_selected_move_features(EvaluatorAI& evaluator, const Board& board, Player player) {
    const AIInput input{&board, player, {}};
    const AIInspection inspection = evaluator.inspect(input);
    const auto legal_moves = rules::legal_moves(board, player);
    KADOKA_REQUIRE(!legal_moves.empty());
    KADOKA_REQUIRE(inspection.candidates.size() == legal_moves.size());
    KADOKA_REQUIRE(std::find(legal_moves.begin(), legal_moves.end(), inspection.output.move) != legal_moves.end());

    const auto flips = rules::flips_for_move(board, inspection.output.move, player);
    KADOKA_REQUIRE(!flips.empty());
    Board resulting_board = board;
    KADOKA_REQUIRE(rules::apply_move(resulting_board, inspection.output.move, player));
    const std::size_t expected_opponent_mobility = rules::legal_moves(resulting_board, opponent(player)).size();
    const double expected_disc_difference = static_cast<double>(resulting_board.count(to_cell(player))) -
                                            static_cast<double>(resulting_board.count(to_cell(opponent(player))));

    KADOKA_REQUIRE(diagnostic_value(inspection, "flipped_disc_count") == static_cast<double>(flips.size()));
    KADOKA_REQUIRE(diagnostic_value(inspection, "opponent_legal_move_count") ==
                   static_cast<double>(expected_opponent_mobility));
    KADOKA_REQUIRE(std::abs(diagnostic_value(inspection, "opponent_mobility") -
                            static_cast<double>(expected_opponent_mobility) /
                                static_cast<double>(board.size() * board.size())) < 1e-6);
    KADOKA_REQUIRE(diagnostic_value(inspection, "disc_difference_after_move") == expected_disc_difference);
    const double corner_adjacency = diagnostic_value(inspection, "adjacent_to_unoccupied_corner");
    KADOKA_REQUIRE(corner_adjacency == 0.0 || corner_adjacency == 1.0);
}

void test_initial_positions(EvaluatorAI& evaluator) {
    for (const std::size_t size : {6U, 8U, 10U}) {
        const Board board(size);
        verify_selected_move_features(evaluator, board, Player::Black);
    }
}

void test_unoccupied_corner_adjacency(EvaluatorAI& evaluator) {
    Board board(6);
    for (std::size_t row = 0; row < board.size(); ++row) {
        for (std::size_t col = 0; col < board.size(); ++col) {
            board.set({row, col}, Cell::Empty);
        }
    }
    board.set({1, 2}, Cell::White);
    board.set({1, 3}, Cell::Black);
    const auto legal_moves = rules::legal_moves(board, Player::Black);
    KADOKA_REQUIRE(legal_moves.size() == 1U);
    KADOKA_REQUIRE(legal_moves.front() == (Position{1, 1}));

    verify_selected_move_features(evaluator, board, Player::Black);
    const AIInspection inspection = evaluator.inspect(AIInput{&board, Player::Black, {}});
    KADOKA_REQUIRE(diagnostic_value(inspection, "adjacent_to_unoccupied_corner") == 1.0);
}

void test_reference_evaluator_prefers_available_corner(EvaluatorAI& evaluator) {
    Board board(6);
    for (std::size_t row = 0; row < board.size(); ++row) {
        for (std::size_t col = 0; col < board.size(); ++col) {
            board.set({row, col}, Cell::Empty);
        }
    }
    board.set({0, 1}, Cell::White);
    board.set({0, 2}, Cell::Black);
    board.set({2, 4}, Cell::White);
    board.set({3, 4}, Cell::Black);

    const auto legal_moves = rules::legal_moves(board, Player::Black);
    KADOKA_REQUIRE(std::find(legal_moves.begin(), legal_moves.end(), Position{0, 0}) != legal_moves.end());
    KADOKA_REQUIRE(legal_moves.size() > 1U);

    const AIInspection inspection = evaluator.inspect(AIInput{&board, Player::Black, {}});
    KADOKA_REQUIRE(inspection.output.move == (Position{0, 0}));
}

} // namespace

int main(int argc, char** argv) {
    try {
        KADOKA_REQUIRE(argc == 2);
        const ModelRootDescriptor model = load_model_root_descriptor(argv[1]);
        EvaluatorAI evaluator(model.model_id, model);
        test_initial_positions(evaluator);
        test_unoccupied_corner_adjacency(evaluator);
        test_reference_evaluator_prefers_available_corner(evaluator);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
