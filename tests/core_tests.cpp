#include <cassert>
#include <cstddef>

#include "kadoka_othello/game.hpp"

using kadoka::othello::Cell;
using kadoka::othello::Game;
using kadoka::othello::GameStatus;
using kadoka::othello::Player;
using kadoka::othello::Position;

namespace {

void test_initial_board_sizes() {
    for (const std::size_t size : {6U, 8U, 10U}) {
        Game game(size);
        assert(game.board().size() == size);
        assert(game.board().count(Cell::Black) == 2);
        assert(game.board().count(Cell::White) == 2);
        assert(game.status() == GameStatus::Playing);
    }
}

void test_initial_legal_moves() {
    Game game(8);
    const auto moves = game.legal_moves();
    assert(moves.size() == 4);
}

void test_apply_move_and_flip() {
    Game game(8);
    const Position move{2, 3};

    assert(game.play(move));
    assert(game.board().at(move) == Cell::Black);
    assert(game.board().at({3, 3}) == Cell::Black);
    assert(game.board().count(Cell::Black) == 4);
    assert(game.board().count(Cell::White) == 1);
    assert(game.current_player() == Player::White);
    assert(game.history().size() == 1);
}

void test_illegal_move_does_not_advance_turn() {
    Game game(8);
    assert(!game.play({0, 0}));
    assert(game.current_player() == Player::Black);
    assert(game.history().empty());
}

void test_reset() {
    Game game(6);
    assert(game.play({1, 2}));
    game.reset();

    assert(game.current_player() == Player::Black);
    assert(game.history().empty());
    assert(game.board().count(Cell::Black) == 2);
    assert(game.board().count(Cell::White) == 2);
}

}  // namespace

int main() {
    test_initial_board_sizes();
    test_initial_legal_moves();
    test_apply_move_and_flip();
    test_illegal_move_does_not_advance_turn();
    test_reset();
    return 0;
}
