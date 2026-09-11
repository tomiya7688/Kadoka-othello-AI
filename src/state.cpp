#include "kadoka_othello/state.hpp"

#include <sstream>

namespace kadoka::othello {
namespace {

const char* player_name(Player player) { return player == Player::Black ? "black" : "white"; }
const char* status_name(GameStatus status) { return status == GameStatus::Playing ? "playing" : "finished"; }
const char* winner_name(Winner winner) {
    if (winner == Winner::Black) return "black";
    if (winner == Winner::White) return "white";
    return "draw";
}

}  // namespace

GameSnapshot make_snapshot(const Game& game) {
    GameSnapshot snapshot;
    snapshot.board_size = game.board().size();
    snapshot.current_player = game.current_player();
    snapshot.status = game.status();
    snapshot.legal_moves = game.legal_moves();
    snapshot.history = game.history();
    snapshot.result = game.result();
    snapshot.cells.reserve(snapshot.board_size * snapshot.board_size);
    for (std::size_t row = 0; row < snapshot.board_size; ++row) {
        for (std::size_t col = 0; col < snapshot.board_size; ++col) {
            snapshot.cells.push_back(game.board().at({row, col}));
        }
    }
    return snapshot;
}

std::string snapshot_to_json(const GameSnapshot& snapshot) {
    std::ostringstream out;
    out << "{\"board_size\":" << snapshot.board_size
        << ",\"current_player\":\"" << player_name(snapshot.current_player) << "\""
        << ",\"status\":\"" << status_name(snapshot.status) << "\""
        << ",\"cells\":[";
    for (std::size_t i = 0; i < snapshot.cells.size(); ++i) {
        if (i) out << ',';
        out << static_cast<int>(snapshot.cells[i]);
    }
    out << "],\"legal_moves\":[";
    for (std::size_t i = 0; i < snapshot.legal_moves.size(); ++i) {
        if (i) out << ',';
        out << "{\"row\":" << snapshot.legal_moves[i].row
            << ",\"col\":" << snapshot.legal_moves[i].col << '}';
    }
    out << "],\"history\":[";
    for (std::size_t i = 0; i < snapshot.history.size(); ++i) {
        if (i) out << ',';
        out << "{\"player\":\"" << player_name(snapshot.history[i].player)
            << "\",\"row\":" << snapshot.history[i].position.row
            << ",\"col\":" << snapshot.history[i].position.col << '}';
    }
    out << ']';
    if (snapshot.result) {
        out << ",\"result\":{\"black\":" << snapshot.result->black_discs
            << ",\"white\":" << snapshot.result->white_discs
            << ",\"winner\":\"" << winner_name(snapshot.result->winner) << "\"}";
    } else {
        out << ",\"result\":null";
    }
    out << '}';
    return out.str();
}

}  // namespace kadoka::othello
