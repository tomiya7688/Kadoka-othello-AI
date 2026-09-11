#include <cassert>
#include <cstddef>
#include <sstream>

#include "kadoka_othello/ai.hpp"
#include "kadoka_othello/headless.hpp"
#include "kadoka_othello/state.hpp"

using namespace kadoka::othello;

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
    assert(game.legal_moves().size() == 4);
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

void test_snapshot() {
    Game game(8);
    const auto snapshot = make_snapshot(game);
    assert(snapshot.board_size == 8);
    assert(snapshot.cells.size() == 64);
    assert(snapshot.legal_moves.size() == 4);
    assert(snapshot_to_json(snapshot).find("\"board_size\":8") != std::string::npos);
}

void test_ai_adapters() {
    Game game(8);
    const auto legal_moves = game.legal_moves();
    const AIInput input{&game.board(), &legal_moves};

    PassThroughAdapter pass_through;
    const auto passed = pass_through.adapt(input);
    assert(passed.board == &game.board());
    assert(passed.legal_moves == &legal_moves);

    DropLegalMovesAdapter drop_legal_moves;
    const auto dropped = drop_legal_moves.adapt(input);
    assert(dropped.board == &game.board());
    assert(dropped.legal_moves == nullptr);
}

void test_random_ai_protocol() {
    Game game(8);
    const auto legal_moves = game.legal_moves();
    RandomAI random_ai(12345);
    PassThroughAdapter adapter;

    const AIOutput output = invoke_ai(
        AIPackage{&random_ai, &adapter},
        AIInput{&game.board(), &legal_moves});

    bool found = false;
    for (const Position move : legal_moves) {
        if (move == output.move) {
            found = true;
            break;
        }
    }
    assert(found);
}

void test_headless_runner() {
    HeadlessConfig config;
    config.board_size = 6;
    config.games = 4;
    config.seed = 12345;
    std::ostringstream output;
    const auto summary = run_random_games(config, &output);
    assert(summary.games == 4);
    assert(summary.black_wins + summary.white_wins + summary.draws == 4);
    assert(summary.invalid_move_attempts == 0);
    assert(!output.str().empty());
}

}  // namespace

int main() {
    test_initial_board_sizes();
    test_initial_legal_moves();
    test_apply_move_and_flip();
    test_illegal_move_does_not_advance_turn();
    test_snapshot();
    test_ai_adapters();
    test_random_ai_protocol();
    test_headless_runner();
    return 0;
}
