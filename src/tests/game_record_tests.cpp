#include <cassert>
#include <cstddef>
#include <sstream>
#include <string>
#include <vector>

#include "kadoka_othello/game_record.hpp"

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
    assert(id.size() == 26);
    const std::string alphabet = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";
    for (const char ch : id) {
        assert(alphabet.find(ch) != std::string::npos);
    }
    assert(alphabet.find(id.front()) < 8);
}

void test_record_events_and_round_trip() {
    Game game(4);
    const std::string game_id = "01ARZ3NDEKTSV4RRFFQ69G5FAV";
    GameRecordRecorder recorder(game, game_id);

    assert(recorder.record().board_states.size() == 1);
    assert(recorder.record().board_states.front().ply == 0);

    const std::string initial_cells =
        board_state_record_to_json(recorder.record().board_states.front());

    assert(!game.play({0, 0}));
    assert(game.ply() == 0);
    assert(recorder.record().board_states.size() == 1);
    assert(recorder.record().aux_events.size() == 2);
    assert(recorder.record().aux_events[0].event_type == "illegal_move");
    assert(recorder.record().aux_events[0].ply == 0);
    assert(recorder.record().aux_events[1].event_type == "invalid_move_notification");
    assert(recorder.record().aux_events[1].ply == 0);
    assert(recorder.record().aux_events[0].event_index == 0);
    assert(recorder.record().aux_events[1].event_index == 1);

    const std::vector<Position> sequence{
        {0, 1}, {0, 0}, {1, 0}, {0, 2},
        {0, 3}, {2, 0}, {3, 0}, {1, 3},
        {2, 3}, {3, 1}, {3, 2}, {3, 3},
    };
    for (const Position move : sequence) {
        assert(game.play(move));
    }

    assert(game.status() == GameStatus::Playing);
    assert(game.can_pass());
    const std::size_t before_pass_ply = game.ply();
    const CoreState before_pass = recorder.record().board_states.back().state;
    const Player before_pass_player = game.current_player();

    assert(game.pass());
    assert(game.ply() == before_pass_ply + 1);
    assert(game.current_player() == opponent(before_pass_player));
    assert(recorder.record().board_states.back().ply == game.ply());
    assert(recorder.record().board_states.back().state.cells == before_pass.cells);
    assert(recorder.record().board_states.back().state.side_to_move == game.current_player());

    while (game.status() == GameStatus::Playing) {
        if (game.can_pass()) {
            assert(game.pass());
            continue;
        }
        const auto legal = game.legal_moves();
        assert(!legal.empty());
        assert(game.play(legal.front()));
    }

    const GameRecord& record = recorder.record();
    assert(!record.aux_events.empty());
    assert(record.aux_events.back().event_type == "terminal");
    assert(record.aux_events.back().terminal.has_value());
    assert(record.aux_events.back().terminal->black_discs +
           record.aux_events.back().terminal->white_discs == 16);

    for (std::size_t i = 0; i < record.aux_events.size(); ++i) {
        assert(record.aux_events[i].event_index == i);
        assert(record.aux_events[i].game_id == game_id);
        assert(has_board_state_at_ply(record, record.aux_events[i].ply));
    }

    std::ostringstream board_jsonl;
    std::ostringstream aux_jsonl;
    write_game_record_jsonl(record, board_jsonl, aux_jsonl);

    std::istringstream board_input(board_jsonl.str());
    std::istringstream aux_input(aux_jsonl.str());
    const GameRecord parsed = read_game_record_jsonl(board_input, aux_input);
    assert(parsed.game_id == game_id);
    assert(parsed.board_states.size() == record.board_states.size());
    assert(parsed.aux_events.size() == record.aux_events.size());
    assert(parsed.board_states.front().ply == 0);
    assert(parsed.aux_events.front().event_type == "illegal_move");
    assert(parsed.aux_events.back().terminal.has_value());

    std::string future_board = initial_cells;
    future_board.insert(future_board.size() - 1, ",\"future_optional\":{\"x\":1}");
    const BoardStateRecord future_parsed = parse_board_state_record_json(future_board);
    assert(future_parsed.game_id == game_id);

    std::string future_aux = game_aux_record_to_json(record.aux_events.front());
    future_aux.insert(future_aux.size() - 1, ",\"future_optional\":true");
    const GameAuxRecord future_aux_parsed = parse_game_aux_record_json(future_aux);
    assert(future_aux_parsed.event_type == "illegal_move");
}

void test_board_sizes() {
    for (const std::size_t size : {6U, 8U, 10U}) {
        Game game(size);
        GameRecordRecorder recorder(game);
        const auto& state = recorder.record().board_states.front();
        assert(state.state.board_size == size);
        assert(state.state.cells.size() == size * size);

        const std::string json = board_state_record_to_json(state);
        const BoardStateRecord parsed = parse_board_state_record_json(json);
        assert(parsed.state.board_size == size);
        assert(parsed.state.cells == state.state.cells);
    }
}

}  // namespace

int main() {
    test_ulid_shape();
    test_record_events_and_round_trip();
    test_board_sizes();
    return 0;
}
