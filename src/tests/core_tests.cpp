#include <cstddef>
#include <sstream>
#include <string>

#include "kadoka_othello/ai.hpp"
#include "kadoka_othello/core_state.hpp"
#include "kadoka_othello/headless.hpp"
#include "kadoka_othello/obake_kadoka.hpp"
#include "kadoka_othello/state.hpp"

#include "test_support.hpp"

using namespace kadoka::othello;

namespace {

void test_initial_board_sizes() {
    for (const std::size_t size : {6U, 8U, 10U}) {
        Game game(size);
        KADOKA_REQUIRE(game.board().size() == size);
        KADOKA_REQUIRE(game.board().count(Cell::Black) == 2);
        KADOKA_REQUIRE(game.board().count(Cell::White) == 2);
        KADOKA_REQUIRE(game.status() == GameStatus::Playing);
    }
}

void test_initial_legal_moves() {
    Game game(8);
    KADOKA_REQUIRE(game.legal_moves().size() == 4);
}

void test_apply_move_and_flip() {
    Game game(8);
    const Position move{2, 3};
    KADOKA_REQUIRE(game.play(move));
    KADOKA_REQUIRE(game.board().at(move) == Cell::Black);
    KADOKA_REQUIRE(game.board().at({3, 3}) == Cell::Black);
    KADOKA_REQUIRE(game.board().count(Cell::Black) == 4);
    KADOKA_REQUIRE(game.board().count(Cell::White) == 1);
    KADOKA_REQUIRE(game.current_player() == Player::White);
    KADOKA_REQUIRE(game.history().size() == 1);
    KADOKA_REQUIRE(game.ply() == 1);
}

void test_illegal_move_event_does_not_advance_state() {
    Game game(8);
    std::size_t invalid_events = 0;
    std::size_t event_ply = 999;
    Player event_actor = Player::White;
    Position event_move{99, 99};

    static_cast<void>(game.add_event_listener([&](const GameEvent& event) {
        if (event.type != GameEventType::InvalidMove) return;
        ++invalid_events;
        event_ply = event.ply;
        event_actor = event.actor;
        KADOKA_REQUIRE(event.action.has_value());
        event_move = *event.action;
    }));

    const std::string before = snapshot_to_json(make_snapshot(game));
    KADOKA_REQUIRE(!game.play({0, 0}));
    const std::string after = snapshot_to_json(make_snapshot(game));

    KADOKA_REQUIRE(before == after);
    KADOKA_REQUIRE(game.current_player() == Player::Black);
    KADOKA_REQUIRE(game.history().empty());
    KADOKA_REQUIRE(game.ply() == 0);
    KADOKA_REQUIRE(invalid_events == 1);
    KADOKA_REQUIRE(event_ply == 0);
    KADOKA_REQUIRE(event_actor == Player::Black);
    KADOKA_REQUIRE((event_move == Position{0, 0}));
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

        KADOKA_REQUIRE(json.find("\"format\":\"kadoka.core_state.v1\"") != std::string::npos);
        KADOKA_REQUIRE(json.find("\"side_to_move\":\"black\"") != std::string::npos);
        KADOKA_REQUIRE(json.find("\"legal_moves\"") == std::string::npos);
        KADOKA_REQUIRE(json.find("\"history\"") == std::string::npos);
        KADOKA_REQUIRE(json.find("\"status\"") == std::string::npos);
        KADOKA_REQUIRE(json.find("\"result\"") == std::string::npos);

        const CoreState parsed = parse_core_state_json(json);
        KADOKA_REQUIRE(parsed.board_size == size);
        KADOKA_REQUIRE(parsed.side_to_move == Player::Black);
        KADOKA_REQUIRE(parsed.cells.size() == size * size);
        KADOKA_REQUIRE(parsed.time.black_remaining_ms == 300000);
        KADOKA_REQUIRE(parsed.time.white_remaining_ms == 299500);
        KADOKA_REQUIRE(parsed.time.move_limit_ms == 5000);

        const Board restored = board_from_core_state(parsed);
        for (std::size_t row = 0; row < size; ++row) {
            for (std::size_t col = 0; col < size; ++col) {
                KADOKA_REQUIRE(restored.at({row, col}) == game.board().at({row, col}));
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
    KADOKA_REQUIRE(found);
}

void test_obake_kadoka_protocol() {
    Game game(8);
    ObakeKadokaAI kadoka(12345);

    const AIInspection inspection = inspect_ai(
        AIPackage{&kadoka},
        AIInput{&game.board(), game.current_player(), {}});

    KADOKA_REQUIRE(game.board().at(inspection.output.move) == Cell::Empty);
    KADOKA_REQUIRE(inspection.candidates.size() == 60);
}

void test_headless_runner() {
    HeadlessConfig config;
    config.board_size = 6;
    config.games = 4;
    config.seed = 12345;
    std::ostringstream output;
    const auto summary = run_random_games(config, &output);
    KADOKA_REQUIRE(summary.games == 4);
    KADOKA_REQUIRE(summary.black_wins + summary.white_wins + summary.draws == 4);
    KADOKA_REQUIRE(summary.invalid_move_attempts == 0);
    KADOKA_REQUIRE(output.str().find("kadoka.core_state.v1") != std::string::npos);
    KADOKA_REQUIRE(output.str().find("\"legal_moves\"") == std::string::npos);
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
