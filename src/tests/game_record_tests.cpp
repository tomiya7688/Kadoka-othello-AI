#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include "kadoka_othello/game_record.hpp"

#include "test_support.hpp"

using namespace kadoka::othello;

namespace {

bool has_board_state_at_ply(const GameRecord& record, std::size_t ply) {
    for (const auto& state : record.board_states) {
        if (state.ply == ply && state.game_id == record.game_id) return true;
    }
    return false;
}

void test_ulid_shape() {
    const std::string id = generate_game_ulid();
    KADOKA_REQUIRE(id.size() == 26);
    const std::string alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    for (const char ch : id) {
        KADOKA_REQUIRE(alphabet.find(ch) != std::string::npos);
    }
    KADOKA_REQUIRE(alphabet.find(id.front()) < 8);
}

void test_record_events_and_round_trip() {
    Game game(4);
    const std::string game_id = "01ARZ3NDEKTSV4RRFFQ69G5FAV";
    GameRecordRecorder recorder(game, game_id);

    KADOKA_REQUIRE(recorder.record().board_states.size() == 1);
    KADOKA_REQUIRE(recorder.record().board_states.front().ply == 0);

    const std::string initial_cells =
        board_state_record_to_json(recorder.record().board_states.front());

    KADOKA_REQUIRE(!game.play({0, 0}));
    KADOKA_REQUIRE(game.ply() == 0);
    KADOKA_REQUIRE(recorder.record().board_states.size() == 1);
    KADOKA_REQUIRE(recorder.record().aux_events.size() == 2);
    KADOKA_REQUIRE(recorder.record().aux_events[0].event_type == "illegal_move");
    KADOKA_REQUIRE(recorder.record().aux_events[0].ply == 0);
    KADOKA_REQUIRE(recorder.record().aux_events[1].event_type == "invalid_move_notification");
    KADOKA_REQUIRE(recorder.record().aux_events[1].ply == 0);
    KADOKA_REQUIRE(recorder.record().aux_events[0].event_index == 0);
    KADOKA_REQUIRE(recorder.record().aux_events[1].event_index == 1);

    const std::vector<Position> sequence{
        {0, 1}, {0, 0}, {1, 0}, {0, 2},
        {0, 3}, {2, 0}, {3, 0}, {2, 3},
    };
    for (const Position move : sequence) {
        KADOKA_REQUIRE(game.play(move));
    }

    KADOKA_REQUIRE(game.status() == GameStatus::Playing);
    KADOKA_REQUIRE(game.can_pass());
    const std::size_t before_pass_ply = game.ply();
    const CoreState before_pass = recorder.record().board_states.back().state;
    const Player before_pass_player = game.current_player();

    KADOKA_REQUIRE(game.pass());
    KADOKA_REQUIRE(game.ply() == before_pass_ply + 1);
    KADOKA_REQUIRE(game.current_player() == opponent(before_pass_player));
    KADOKA_REQUIRE(recorder.record().board_states.back().ply == game.ply());
    KADOKA_REQUIRE(recorder.record().board_states.back().state.cells == before_pass.cells);
    KADOKA_REQUIRE(recorder.record().board_states.back().state.side_to_move == game.current_player());

    while (game.status() == GameStatus::Playing) {
        if (game.can_pass()) {
            KADOKA_REQUIRE(game.pass());
            continue;
        }
        const auto legal = game.legal_moves();
        KADOKA_REQUIRE(!legal.empty());
        KADOKA_REQUIRE(game.play(legal.front()));
    }

    const GameRecord& record = recorder.record();
    KADOKA_REQUIRE(!record.aux_events.empty());
    KADOKA_REQUIRE(record.aux_events.back().event_type == "terminal");
    KADOKA_REQUIRE(record.aux_events.back().terminal.has_value());
    KADOKA_REQUIRE(record.aux_events.back().terminal->black_discs +
           record.aux_events.back().terminal->white_discs == 16);

    for (std::size_t i = 0; i < record.aux_events.size(); ++i) {
        KADOKA_REQUIRE(record.aux_events[i].event_index == i);
        KADOKA_REQUIRE(record.aux_events[i].game_id == game_id);
        KADOKA_REQUIRE(has_board_state_at_ply(record, record.aux_events[i].ply));
    }

    std::ostringstream board_jsonl;
    std::ostringstream aux_jsonl;
    write_game_record_jsonl(record, board_jsonl, aux_jsonl);

    std::istringstream board_input(board_jsonl.str());
    std::istringstream aux_input(aux_jsonl.str());
    const GameRecord parsed = read_game_record_jsonl(board_input, aux_input);
    KADOKA_REQUIRE(parsed.game_id == game_id);
    KADOKA_REQUIRE(parsed.board_states.size() == record.board_states.size());
    KADOKA_REQUIRE(parsed.aux_events.size() == record.aux_events.size());
    KADOKA_REQUIRE(parsed.board_states.front().ply == 0);
    KADOKA_REQUIRE(parsed.aux_events.front().event_type == "illegal_move");
    KADOKA_REQUIRE(parsed.aux_events.back().terminal.has_value());

    std::string future_board = initial_cells;
    future_board.insert(future_board.size() - 1, ",\"future_optional\":{\"x\":1}");
    const BoardStateRecord future_parsed = parse_board_state_record_json(future_board);
    KADOKA_REQUIRE(future_parsed.game_id == game_id);

    std::string future_aux = game_aux_record_to_json(record.aux_events.front());
    future_aux.insert(future_aux.size() - 1, ",\"future_optional\":true");
    const GameAuxRecord future_aux_parsed = parse_game_aux_record_json(future_aux);
    KADOKA_REQUIRE(future_aux_parsed.event_type == "illegal_move");
}

void test_board_sizes() {
    for (const std::size_t size : {6U, 8U, 10U}) {
        Game game(size);
        GameRecordRecorder recorder(game);
        const auto& state = recorder.record().board_states.front();
        KADOKA_REQUIRE(state.state.board_size == size);
        KADOKA_REQUIRE(state.state.cells.size() == size * size);

        const std::string json = board_state_record_to_json(state);
        const BoardStateRecord parsed = parse_board_state_record_json(json);
        KADOKA_REQUIRE(parsed.state.board_size == size);
        KADOKA_REQUIRE(parsed.state.cells == state.state.cells);
    }
}

}  // namespace

int main() {
    test_ulid_shape();
    test_record_events_and_round_trip();
    test_board_sizes();
    return 0;
}
