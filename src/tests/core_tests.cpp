#include <cassert>
#include <cstddef>
#include <sstream>
#include <string>

#include "kadoka_othello/ai.hpp"
#include "kadoka_othello/core_state.hpp"
#include "kadoka_othello/headless.hpp"
#include "kadoka_othello/obake_kadoka.hpp"
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
    assert(game.ply() == 1);
}

void test_illegal_move_event_does_not_advance_state() {
    Game game(8);
    std::size_t invalid_events = 0;
    std::size_t event_ply = 999;
    Player event_actor = Player::White;
    Position event_move{99, 99};

    game.add_event_listener([&](const GameEvent& event) {
        if (event.type != GameEventType::InvalidMove) return;
        ++invalid_events;
        event_ply = event.ply;
        event_actor = event.actor;
        assert(event.action.has_value());
        event_move = *event.action;
    });

    const std::string before = snapshot_to_json(make_snapshot(game));
    assert(!game.play({0, 0}));
    const std::string after = snapshot_to_json(make_snapshot(game));

    assert(before == after);
    assert(game.current_player() == Player::Black);
    assert(game.history().empty());
    assert(game.ply() == 0);
    assert(invalid_events == 1);
    assert(event_ply == 0);
    assert(event_actor == Player::Black);
    assert((event_move == Position{0, 0}));
}

void test_core_state_json_round_trip() {
    for (const std::size_t size : {6U, 8U, 10U}) {
        Game game(size);
        CoreTimeState time;
        time.black_remaining_ms = 300000;
        time.white_remaining_ms = 299500;
        time.move_limit_ms = 5000;

        const CoreStateView view{&game.board(), game.current_player(), time};
        const std::string json = core_state_to_json(view);

        assert(json.find("\"format\":\"kadoka.core_state.v1\"") != std::string::npos);
        assert(json.find("\"side_to_move\":\"black\"") != std::string::npos);
        assert(json.find("\"legal_moves\"") == std::string::npos);
        assert(json.find("\"history\"") == std::string::npos);
        assert(json.find("\"status\"") == std::string::npos);
        assert(json.find("\"result\"") == std::string::npos);

        const CoreState parsed = parse_core_state_json(json);
        assert(parsed.board_size == size);
        assert(parsed.side_to_move == Player::Black);
        assert(parsed.cells.size() == size * size);
        assert(parsed.time.black_remaining_ms == 300000);
        assert(parsed.time.white_remaining_ms == 299500);
        assert(parsed.time.move_limit_ms == 5000);

        const Board restored = board_from_core_state(parsed);
        for (std::size_t row = 0; row < size; ++row) {
            for (std::size_t col = 0; col < size; ++col) {
                assert(restored.at({row, col}) == game.board().at({row, col}));
            }
        }
    }
}

void test_random_ai_protocol() {
    Game game(8);
    const auto legal_moves = game.legal_moves();
    RandomAI random_ai(12345);

    const AIOutput output = invoke_ai(
        AIPackage{&random_ai},
        AIInput{&game.board(), game.current_player(), {}});

    bool found = false;
    for (const Position move : legal_moves) {
        if (move == output.move) {
            found = true;
            break;
        }
    }
    assert(found);
}

void test_obake_kadoka_protocol() {
    Game game(8);
    ObakeKadokaAI kadoka(12345);

    const AIInspection inspection = inspect_ai(
        AIPackage{&kadoka},
        AIInput{&game.board(), game.current_player(), {}});

    assert(game.board().at(inspection.output.move) == Cell::Empty);
    assert(inspection.candidates.size() == 60);
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
    assert(output.str().find("kadoka.core_state.v1") != std::string::npos);
    assert(output.str().find("\"legal_moves\"") == std::string::npos);
}

}  // namespace

int main() {
    test_initial_board_sizes();
    test_initial_legal_moves();
    test_apply_move_and_flip();
    test_illegal_move_event_does_not_advance_state();
    test_core_state_json_round_trip();
    test_random_ai_protocol();
    test_obake_kadoka_protocol();
    test_headless_runner();
    return 0;
}
