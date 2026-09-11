#include "kadoka_othello/model_data.hpp"

#include <iomanip>
#include <ostream>
#include <sstream>

namespace kadoka::othello {
namespace {

std::string escape_json(const std::string& value) {
    std::string result;
    result.reserve(value.size());
    for (const char ch : value) {
        switch (ch) {
            case '\\': result += "\\\\"; break;
            case '"': result += "\\\""; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += ch; break;
        }
    }
    return result;
}

const char* player_name(Player player) {
    return player == Player::Black ? "black" : "white";
}

int cell_value(Cell cell) {
    return static_cast<int>(cell);
}

void write_position(std::ostream& output, Position position) {
    output << "{\"row\":" << position.row << ",\"col\":" << position.col << '}';
}

}  // namespace

std::string KadokaJsonlCodec::id() const {
    return "kadoka.jsonl.v1";
}

void KadokaJsonlCodec::write(std::ostream& output, const ModelRecord& record) const {
    output << '{';
    output << "\"format_version\":\"" << escape_json(record.format_version) << "\",";
    output << "\"model_id\":\"" << escape_json(record.model_id) << "\",";
    output << "\"game_id\":\"" << escape_json(record.game_id) << "\",";
    output << "\"ply\":" << record.ply << ',';
    output << "\"board_size\":" << record.board_size << ',';
    output << "\"player\":\"" << player_name(record.player) << "\",";

    output << "\"board\":[";
    for (std::size_t i = 0; i < record.board.size(); ++i) {
        if (i != 0) output << ',';
        output << cell_value(record.board[i]);
    }
    output << "],";

    output << "\"legal_moves\":[";
    for (std::size_t i = 0; i < record.legal_moves.size(); ++i) {
        if (i != 0) output << ',';
        write_position(output, record.legal_moves[i]);
    }
    output << "],";

    output << "\"selected_move\":";
    write_position(output, record.selected_move);
    output << ',';

    output << "\"candidates\":[";
    for (std::size_t i = 0; i < record.candidates.size(); ++i) {
        if (i != 0) output << ',';
        output << '{' << "\"move\":";
        write_position(output, record.candidates[i].move);
        output << ",\"value\":" << std::setprecision(17) << record.candidates[i].value;
        output << ",\"policy\":" << std::setprecision(17) << record.candidates[i].policy << '}';
    }
    output << "],";

    output << "\"diagnostics\":{";
    for (std::size_t i = 0; i < record.diagnostics.size(); ++i) {
        if (i != 0) output << ',';
        output << '"' << escape_json(record.diagnostics[i].key) << "\":\""
               << escape_json(record.diagnostics[i].value) << '"';
    }
    output << "}}\n";
}

ModelRecord make_model_record(
    const Game& game,
    const std::string& model_id,
    const std::string& game_id,
    const AIInspection& inspection) {
    ModelRecord record;
    record.model_id = model_id;
    record.game_id = game_id;
    record.ply = game.history().size();
    record.board_size = game.board().size();
    record.player = game.current_player();
    record.legal_moves = game.legal_moves();
    record.selected_move = inspection.output.move;
    record.diagnostics = inspection.diagnostics;
    for (const auto& candidate : inspection.candidates) {
        record.candidates.push_back({candidate.move, candidate.value, candidate.policy});
    }

    record.board.reserve(record.board_size * record.board_size);
    for (std::size_t row = 0; row < record.board_size; ++row) {
        for (std::size_t col = 0; col < record.board_size; ++col) {
            record.board.push_back(game.board().at({row, col}));
        }
    }

    return record;
}

}  // namespace kadoka::othello
