#include "kadoka_othello/core_state.hpp"

#include <cctype>
#include <sstream>
#include <stdexcept>

namespace kadoka::othello {
namespace {

const char* player_name(Player player) noexcept {
    return player == Player::Black ? "black" : "white";
}

Player parse_player(std::string_view value) {
    if (value == "black") return Player::Black;
    if (value == "white") return Player::White;
    throw std::invalid_argument("core state side_to_move must be black or white");
}

std::size_t find_value_start(std::string_view json, std::string_view key) {
    const std::string token = "\"" + std::string(key) + "\"";
    const std::size_t key_pos = json.find(token);
    if (key_pos == std::string_view::npos) {
        throw std::invalid_argument("core state missing field: " + std::string(key));
    }
    const std::size_t colon = json.find(':', key_pos + token.size());
    if (colon == std::string_view::npos) {
        throw std::invalid_argument("core state malformed field: " + std::string(key));
    }
    std::size_t pos = colon + 1;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos]))) ++pos;
    return pos;
}

std::string read_string(std::string_view json, std::string_view key) {
    std::size_t pos = find_value_start(json, key);
    if (pos >= json.size() || json[pos] != '"') {
        throw std::invalid_argument("core state field is not a string: " + std::string(key));
    }
    const std::size_t end = json.find('"', pos + 1);
    if (end == std::string_view::npos) {
        throw std::invalid_argument("core state unterminated string: " + std::string(key));
    }
    return std::string(json.substr(pos + 1, end - pos - 1));
}

std::uint64_t read_uint(std::string_view json, std::string_view key) {
    std::size_t pos = find_value_start(json, key);
    const std::size_t begin = pos;
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) ++pos;
    if (pos == begin) {
        throw std::invalid_argument("core state field is not an unsigned integer: " + std::string(key));
    }
    return std::stoull(std::string(json.substr(begin, pos - begin)));
}

std::optional<std::uint64_t> read_optional_uint(
    std::string_view object,
    std::string_view key) {
    std::size_t pos = find_value_start(object, key);
    if (object.substr(pos, 4) == "null") return std::nullopt;
    const std::size_t begin = pos;
    while (pos < object.size() && std::isdigit(static_cast<unsigned char>(object[pos]))) ++pos;
    if (pos == begin) {
        throw std::invalid_argument("core state time field must be unsigned integer or null");
    }
    return std::stoull(std::string(object.substr(begin, pos - begin)));
}

std::string_view read_object(std::string_view json, std::string_view key) {
    const std::size_t start = find_value_start(json, key);
    if (start >= json.size() || json[start] != '{') {
        throw std::invalid_argument("core state field is not an object: " + std::string(key));
    }
    std::size_t depth = 0;
    for (std::size_t pos = start; pos < json.size(); ++pos) {
        if (json[pos] == '{') ++depth;
        else if (json[pos] == '}') {
            if (--depth == 0) return json.substr(start, pos - start + 1);
        }
    }
    throw std::invalid_argument("core state object is not closed: " + std::string(key));
}

std::vector<Cell> read_cells(std::string_view json, std::size_t board_size) {
    std::size_t pos = find_value_start(json, "cells");
    if (pos >= json.size() || json[pos] != '[') {
        throw std::invalid_argument("core state cells must be an array");
    }
    ++pos;

    std::vector<Cell> cells;
    cells.reserve(board_size * board_size);
    while (pos < json.size()) {
        while (pos < json.size() &&
               (std::isspace(static_cast<unsigned char>(json[pos])) || json[pos] == ',')) {
            ++pos;
        }
        if (pos >= json.size()) break;
        if (json[pos] == ']') {
            ++pos;
            break;
        }
        if (json[pos] < '0' || json[pos] > '2') {
            throw std::invalid_argument("core state cell must be 0, 1 or 2");
        }
        cells.push_back(static_cast<Cell>(json[pos] - '0'));
        ++pos;
    }

    if (cells.size() != board_size * board_size) {
        throw std::invalid_argument("core state cells length does not match board_size");
    }
    return cells;
}

void write_optional_uint(std::ostringstream& out, const std::optional<std::uint64_t>& value) {
    if (value) out << *value;
    else out << "null";
}

std::string serialize(
    std::size_t board_size,
    const std::vector<Cell>& cells,
    Player side_to_move,
    const CoreTimeState& time) {
    if (cells.size() != board_size * board_size) {
        throw std::invalid_argument("core state cells length does not match board_size");
    }

    std::ostringstream out;
    out << "{\"format\":\"" << kCoreStateFormat << "\""
        << ",\"board_size\":" << board_size
        << ",\"cells\":[";
    for (std::size_t i = 0; i < cells.size(); ++i) {
        if (i) out << ',';
        out << static_cast<int>(cells[i]);
    }
    out << "],\"side_to_move\":\"" << player_name(side_to_move) << "\""
        << ",\"time\":{\"black_remaining_ms\":";
    write_optional_uint(out, time.black_remaining_ms);
    out << ",\"white_remaining_ms\":";
    write_optional_uint(out, time.white_remaining_ms);
    out << ",\"move_limit_ms\":";
    write_optional_uint(out, time.move_limit_ms);
    out << "}}";
    return out.str();
}

}  // namespace

CoreState make_core_state(const CoreStateView& view) {
    if (view.board == nullptr) {
        throw std::invalid_argument("core state requires board");
    }
    CoreState state;
    state.board_size = view.board->size();
    state.side_to_move = view.side_to_move;
    state.time = view.time;
    state.cells.reserve(state.board_size * state.board_size);
    for (std::size_t row = 0; row < state.board_size; ++row) {
        for (std::size_t col = 0; col < state.board_size; ++col) {
            state.cells.push_back(view.board->at({row, col}));
        }
    }
    return state;
}

Board board_from_core_state(const CoreState& state) {
    if (state.cells.size() != state.board_size * state.board_size) {
        throw std::invalid_argument("core state cells length does not match board_size");
    }
    Board board(state.board_size);
    for (std::size_t row = 0; row < state.board_size; ++row) {
        for (std::size_t col = 0; col < state.board_size; ++col) {
            board.set({row, col}, state.cells[row * state.board_size + col]);
        }
    }
    return board;
}

std::string core_state_to_json(const CoreStateView& view) {
    const CoreState state = make_core_state(view);
    return core_state_to_json(state);
}

std::string core_state_to_json(const CoreState& state) {
    return serialize(state.board_size, state.cells, state.side_to_move, state.time);
}

CoreState parse_core_state_json(std::string_view json) {
    if (read_string(json, "format") != kCoreStateFormat) {
        throw std::invalid_argument("unsupported core state format");
    }

    CoreState state;
    state.board_size = static_cast<std::size_t>(read_uint(json, "board_size"));
    if (state.board_size < 4 || state.board_size % 2 != 0) {
        throw std::invalid_argument("core state board_size must be even and >= 4");
    }
    state.cells = read_cells(json, state.board_size);
    state.side_to_move = parse_player(read_string(json, "side_to_move"));

    const std::string_view time = read_object(json, "time");
    state.time.black_remaining_ms = read_optional_uint(time, "black_remaining_ms");
    state.time.white_remaining_ms = read_optional_uint(time, "white_remaining_ms");
    state.time.move_limit_ms = read_optional_uint(time, "move_limit_ms");
    return state;
}

}  // namespace kadoka::othello
