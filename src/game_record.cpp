#include "kadoka_othello/game_record.hpp"

#include <array>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace kadoka::othello {
namespace {

constexpr char kUlidAlphabet[] = "0123456789ABCDEFGHJKMNPQRSTVWXYZ";

const char* player_name(Player player) noexcept {
    return player == Player::Black ? "black" : "white";
}

Player parse_player(std::string_view value) {
    if (value == "black") return Player::Black;
    if (value == "white") return Player::White;
    throw std::invalid_argument("record player must be black or white");
}

const char* winner_name(Winner winner) noexcept {
    if (winner == Winner::Black) return "black";
    if (winner == Winner::White) return "white";
    return "draw";
}

Winner parse_winner(std::string_view value) {
    if (value == "black") return Winner::Black;
    if (value == "white") return Winner::White;
    if (value == "draw") return Winner::Draw;
    throw std::invalid_argument("record winner must be black, white or draw");
}

std::string escape_json(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += ch; break;
        }
    }
    return escaped;
}

std::size_t find_value_start(std::string_view json, std::string_view key) {
    const std::string token = "\"" + std::string(key) + "\"";
    std::size_t search_from = 0;
    while (true) {
        const std::size_t key_pos = json.find(token, search_from);
        if (key_pos == std::string_view::npos) {
            throw std::invalid_argument("record missing field: " + std::string(key));
        }

        std::size_t colon = key_pos + token.size();
        while (colon < json.size() &&
               std::isspace(static_cast<unsigned char>(json[colon]))) {
            ++colon;
        }
        if (colon < json.size() && json[colon] == ':') {
            std::size_t pos = colon + 1;
            while (pos < json.size() &&
                   std::isspace(static_cast<unsigned char>(json[pos]))) {
                ++pos;
            }
            return pos;
        }
        search_from = key_pos + token.size();
    }
}

std::string parse_string_at(std::string_view json, std::size_t pos) {
    if (pos >= json.size() || json[pos] != '"') {
        throw std::invalid_argument("record expected JSON string");
    }

    std::string result;
    for (++pos; pos < json.size(); ++pos) {
        const char ch = json[pos];
        if (ch == '"') return result;
        if (ch != '\\') {
            result += ch;
            continue;
        }

        if (++pos >= json.size()) {
            throw std::invalid_argument("record has unterminated JSON escape");
        }
        switch (json[pos]) {
            case '\\': result += '\\'; break;
            case '"': result += '"'; break;
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            default:
                throw std::invalid_argument("record contains unsupported JSON escape");
        }
    }
    throw std::invalid_argument("record has unterminated JSON string");
}

std::string read_string(std::string_view json, std::string_view key) {
    return parse_string_at(json, find_value_start(json, key));
}

std::optional<std::string> read_optional_string(
    std::string_view json,
    std::string_view key) {
    const std::size_t pos = find_value_start(json, key);
    if (json.substr(pos, 4) == "null") return std::nullopt;
    return parse_string_at(json, pos);
}

std::uint64_t read_uint(std::string_view json, std::string_view key) {
    std::size_t pos = find_value_start(json, key);
    const std::size_t begin = pos;
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) ++pos;
    if (begin == pos) {
        throw std::invalid_argument("record field must be an unsigned integer: " + std::string(key));
    }
    return std::stoull(std::string(json.substr(begin, pos - begin)));
}

std::optional<std::uint64_t> read_optional_uint(
    std::string_view json,
    std::string_view key) {
    std::size_t pos = find_value_start(json, key);
    if (json.substr(pos, 4) == "null") return std::nullopt;
    const std::size_t begin = pos;
    while (pos < json.size() && std::isdigit(static_cast<unsigned char>(json[pos]))) ++pos;
    if (begin == pos) {
        throw std::invalid_argument("record time field must be an unsigned integer or null");
    }
    return std::stoull(std::string(json.substr(begin, pos - begin)));
}

std::string_view read_object(std::string_view json, std::string_view key) {
    const std::size_t start = find_value_start(json, key);
    if (start >= json.size() || json[start] != '{') {
        throw std::invalid_argument("record field is not an object: " + std::string(key));
    }

    std::size_t depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (std::size_t pos = start; pos < json.size(); ++pos) {
        const char ch = json[pos];
        if (in_string) {
            if (escaped) escaped = false;
            else if (ch == '\\') escaped = true;
            else if (ch == '"') in_string = false;
            continue;
        }
        if (ch == '"') {
            in_string = true;
            continue;
        }
        if (ch == '{') ++depth;
        else if (ch == '}' && --depth == 0) {
            return json.substr(start, pos - start + 1);
        }
    }
    throw std::invalid_argument("record object is not closed: " + std::string(key));
}

std::optional<std::string_view> read_optional_object(
    std::string_view json,
    std::string_view key) {
    const std::size_t pos = find_value_start(json, key);
    if (json.substr(pos, 4) == "null") return std::nullopt;
    return read_object(json, key);
}

std::vector<Cell> read_cells(std::string_view json, std::size_t board_size) {
    std::size_t pos = find_value_start(json, "cells");
    if (pos >= json.size() || json[pos] != '[') {
        throw std::invalid_argument("board state cells must be an array");
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
        if (json[pos] == ']') break;
        if (json[pos] < '0' || json[pos] > '2') {
            throw std::invalid_argument("board state cell must be 0, 1 or 2");
        }
        cells.push_back(static_cast<Cell>(json[pos] - '0'));
        ++pos;
    }

    if (cells.size() != board_size * board_size) {
        throw std::invalid_argument("board state cells length does not match board_size");
    }
    return cells;
}

void write_optional_uint(
    std::ostringstream& out,
    const std::optional<std::uint64_t>& value) {
    if (value) out << *value;
    else out << "null";
}

void write_time(std::ostringstream& out, const CoreTimeState& time) {
    out << "{\"black_remaining_ms\":";
    write_optional_uint(out, time.black_remaining_ms);
    out << ",\"white_remaining_ms\":";
    write_optional_uint(out, time.white_remaining_ms);
    out << ",\"move_limit_ms\":";
    write_optional_uint(out, time.move_limit_ms);
    out << '}';
}

CoreTimeState parse_time(std::string_view object) {
    CoreTimeState time;
    time.black_remaining_ms = read_optional_uint(object, "black_remaining_ms");
    time.white_remaining_ms = read_optional_uint(object, "white_remaining_ms");
    time.move_limit_ms = read_optional_uint(object, "move_limit_ms");
    return time;
}

std::optional<Player> read_optional_player(
    std::string_view json,
    std::string_view key) {
    const std::size_t pos = find_value_start(json, key);
    if (json.substr(pos, 4) == "null") return std::nullopt;
    return parse_player(parse_string_at(json, pos));
}

std::optional<GameAuxAction> parse_action(std::string_view json) {
    const auto object = read_optional_object(json, "action");
    if (!object) return std::nullopt;

    GameAuxAction action;
    const std::string type = read_string(*object, "type");
    if (type == "move") {
        action.type = GameAuxActionType::Move;
        action.move = Position{
            static_cast<std::size_t>(read_uint(*object, "row")),
            static_cast<std::size_t>(read_uint(*object, "col")),
        };
    } else if (type == "pass") {
        action.type = GameAuxActionType::Pass;
        action.move.reset();
    } else {
        throw std::invalid_argument("unsupported GameAux action type");
    }
    return action;
}

std::optional<GameAuxResult> parse_result(std::string_view json) {
    const auto object = read_optional_object(json, "result");
    if (!object) return std::nullopt;
    return GameAuxResult{
        read_string(*object, "status"),
        read_optional_string(*object, "reason"),
    };
}

std::optional<GameResult> parse_terminal(std::string_view json) {
    const auto object = read_optional_object(json, "terminal");
    if (!object) return std::nullopt;
    return GameResult{
        static_cast<std::size_t>(read_uint(*object, "black_discs")),
        static_cast<std::size_t>(read_uint(*object, "white_discs")),
        parse_winner(read_string(*object, "winner")),
    };
}

GameAuxAction move_action(Position move) {
    return GameAuxAction{GameAuxActionType::Move, move};
}

GameAuxAction pass_action() {
    return GameAuxAction{GameAuxActionType::Pass, std::nullopt};
}

GameAuxResult status_result(std::string status, std::optional<std::string> reason = std::nullopt) {
    return GameAuxResult{std::move(status), std::move(reason)};
}

}  // namespace

std::string generate_game_ulid() {
    std::array<std::uint8_t, 16> bytes{};

    const auto now = std::chrono::system_clock::now().time_since_epoch();
    const std::uint64_t timestamp_ms =
        static_cast<std::uint64_t>(
            std::chrono::duration_cast<std::chrono::milliseconds>(now).count()) &
        0x0000FFFFFFFFFFFFULL;

    for (std::size_t i = 0; i < 6; ++i) {
        bytes[5 - i] = static_cast<std::uint8_t>((timestamp_ms >> (i * 8U)) & 0xFFU);
    }

    thread_local std::mt19937_64 rng([] {
        std::random_device device;
        std::seed_seq seed{
            device(), device(), device(), device(),
            device(), device(), device(), device(),
        };
        return std::mt19937_64(seed);
    }());

    const std::uint64_t random_high = rng();
    const std::uint64_t random_low = rng();
    for (std::size_t i = 0; i < 8; ++i) {
        bytes[6 + i] = static_cast<std::uint8_t>(
            (random_high >> ((7U - i) * 8U)) & 0xFFU);
    }
    bytes[14] = static_cast<std::uint8_t>((random_low >> 8U) & 0xFFU);
    bytes[15] = static_cast<std::uint8_t>(random_low & 0xFFU);

    std::string encoded(26, '0');
    for (std::size_t character = 0; character < encoded.size(); ++character) {
        std::uint8_t value = 0;
        for (std::size_t bit = 0; bit < 5; ++bit) {
            value = static_cast<std::uint8_t>(value << 1U);
            const int source_bit =
                static_cast<int>(character * 5 + bit) - 2;
            if (source_bit < 0 || source_bit >= 128) continue;
            const std::size_t byte_index = static_cast<std::size_t>(source_bit) / 8U;
            const std::size_t bit_index = 7U - (static_cast<std::size_t>(source_bit) % 8U);
            value = static_cast<std::uint8_t>(
                value | ((bytes[byte_index] >> bit_index) & 1U));
        }
        encoded[character] = kUlidAlphabet[value];
    }
    return encoded;
}

std::string board_state_record_to_json(const BoardStateRecord& record) {
    const CoreState& state = record.state;
    if (record.game_id.empty()) {
        throw std::invalid_argument("BoardState record requires game_id");
    }
    if (state.cells.size() != state.board_size * state.board_size) {
        throw std::invalid_argument("BoardState cells length does not match board_size");
    }

    std::ostringstream out;
    out << "{\"schema\":\"" << kBoardStateRecordSchema << "\""
        << ",\"version\":" << kGameRecordVersion
        << ",\"game_id\":\"" << escape_json(record.game_id) << "\""
        << ",\"ply\":" << record.ply
        << ",\"board_size\":" << state.board_size
        << ",\"cells\":[";
    for (std::size_t i = 0; i < state.cells.size(); ++i) {
        if (i) out << ',';
        out << static_cast<int>(state.cells[i]);
    }
    out << "],\"side_to_move\":\"" << player_name(state.side_to_move) << "\""
        << ",\"time\":";
    write_time(out, state.time);
    out << '}';
    return out.str();
}

std::string game_aux_record_to_json(const GameAuxRecord& record) {
    if (record.game_id.empty()) {
        throw std::invalid_argument("GameAux record requires game_id");
    }

    std::ostringstream out;
    out << "{\"schema\":\"" << kGameAuxRecordSchema << "\""
        << ",\"version\":" << kGameRecordVersion
        << ",\"game_id\":\"" << escape_json(record.game_id) << "\""
        << ",\"ply\":" << record.ply
        << ",\"event_index\":" << record.event_index
        << ",\"event_type\":\"" << escape_json(record.event_type) << "\""
        << ",\"actor\":";
    if (record.actor) out << "\"" << player_name(*record.actor) << "\"";
    else out << "null";

    out << ",\"action\":";
    if (!record.action) {
        out << "null";
    } else if (record.action->type == GameAuxActionType::Pass) {
        out << "{\"type\":\"pass\"}";
    } else {
        if (!record.action->move) {
            throw std::invalid_argument("move action requires row/col");
        }
        out << "{\"type\":\"move\",\"row\":" << record.action->move->row
            << ",\"col\":" << record.action->move->col << '}';
    }

    out << ",\"result\":";
    if (!record.result) {
        out << "null";
    } else {
        out << "{\"status\":\"" << escape_json(record.result->status) << "\""
            << ",\"reason\":";
        if (record.result->reason) {
            out << "\"" << escape_json(*record.result->reason) << "\"";
        } else {
            out << "null";
        }
        out << '}';
    }

    out << ",\"terminal\":";
    if (!record.terminal) {
        out << "null";
    } else {
        out << "{\"winner\":\"" << winner_name(record.terminal->winner) << "\""
            << ",\"black_discs\":" << record.terminal->black_discs
            << ",\"white_discs\":" << record.terminal->white_discs
            << '}';
    }
    out << '}';
    return out.str();
}

BoardStateRecord parse_board_state_record_json(const std::string& json) {
    if (read_string(json, "schema") != kBoardStateRecordSchema ||
        read_uint(json, "version") != kGameRecordVersion) {
        throw std::invalid_argument("unsupported BoardState record schema/version");
    }

    BoardStateRecord record;
    record.game_id = read_string(json, "game_id");
    record.ply = static_cast<std::size_t>(read_uint(json, "ply"));
    record.state.board_size = static_cast<std::size_t>(read_uint(json, "board_size"));
    if (record.state.board_size < 4 || record.state.board_size % 2 != 0) {
        throw std::invalid_argument("BoardState board_size must be even and >= 4");
    }
    record.state.cells = read_cells(json, record.state.board_size);
    record.state.side_to_move = parse_player(read_string(json, "side_to_move"));
    record.state.time = parse_time(read_object(json, "time"));
    return record;
}

GameAuxRecord parse_game_aux_record_json(const std::string& json) {
    if (read_string(json, "schema") != kGameAuxRecordSchema ||
        read_uint(json, "version") != kGameRecordVersion) {
        throw std::invalid_argument("unsupported GameAux record schema/version");
    }

    GameAuxRecord record;
    record.game_id = read_string(json, "game_id");
    record.ply = static_cast<std::size_t>(read_uint(json, "ply"));
    record.event_index = static_cast<std::size_t>(read_uint(json, "event_index"));
    record.event_type = read_string(json, "event_type");
    record.actor = read_optional_player(json, "actor");
    record.action = parse_action(json);
    record.result = parse_result(json);
    record.terminal = parse_terminal(json);
    return record;
}

void write_game_record_jsonl(
    const GameRecord& record,
    std::ostream& board_state_output,
    std::ostream& game_aux_output) {
    for (const auto& state : record.board_states) {
        board_state_output << board_state_record_to_json(state) << '\n';
    }
    for (const auto& event : record.aux_events) {
        game_aux_output << game_aux_record_to_json(event) << '\n';
    }
}

std::vector<GameRecord> read_game_records_jsonl(
    std::istream& board_state_input,
    std::istream& game_aux_input) {
    std::vector<GameRecord> records;
    std::unordered_map<std::string, std::size_t> index_by_id;
    std::string line;

    auto ensure_record = [&](const std::string& game_id) -> GameRecord& {
        const auto found = index_by_id.find(game_id);
        if (found != index_by_id.end()) {
            return records[found->second];
        }
        const std::size_t index = records.size();
        records.push_back(GameRecord{game_id, {}, {}});
        index_by_id.emplace(game_id, index);
        return records.back();
    };

    while (std::getline(board_state_input, line)) {
        if (line.empty()) continue;
        BoardStateRecord state = parse_board_state_record_json(line);
        ensure_record(state.game_id).board_states.push_back(std::move(state));
    }

    while (std::getline(game_aux_input, line)) {
        if (line.empty()) continue;
        GameAuxRecord event = parse_game_aux_record_json(line);
        ensure_record(event.game_id).aux_events.push_back(std::move(event));
    }

    for (const auto& record : records) {
        if (record.board_states.empty()) {
            throw std::invalid_argument(
                "GameAux stream contains game_id without BoardState records");
        }
    }
    return records;
}

GameRecord read_game_record_jsonl(
    std::istream& board_state_input,
    std::istream& game_aux_input) {
    std::vector<GameRecord> records =
        read_game_records_jsonl(board_state_input, game_aux_input);
    if (records.size() != 1) {
        throw std::invalid_argument(
            "single Game Record reader requires exactly one game_id");
    }
    return std::move(records.front());
}

GameRecordRecorder::GameRecordRecorder(
    Game& game,
    std::string game_id,
    CoreTimeProvider time_provider)
    : game_(&game),
      time_provider_(std::move(time_provider)) {
    record_.game_id = game_id.empty() ? generate_game_ulid() : std::move(game_id);
    append_board_state(game_->ply());
    listener_id_ = game_->add_event_listener(
        [this](const GameEvent& event) {
            on_event(event);
        });
}

GameRecordRecorder::~GameRecordRecorder() {
    if (game_ != nullptr && listener_id_ != 0) {
        game_->remove_event_listener(listener_id_);
    }
}

const GameRecord& GameRecordRecorder::record() const noexcept {
    return record_;
}

CoreTimeState GameRecordRecorder::current_time() const {
    return time_provider_ ? time_provider_() : CoreTimeState{};
}

void GameRecordRecorder::append_board_state(std::size_t ply) {
    record_.board_states.push_back(BoardStateRecord{
        record_.game_id,
        ply,
        make_core_state(CoreStateView{
            &game_->board(),
            game_->current_player(),
            current_time(),
        }),
    });
}

void GameRecordRecorder::append_aux(
    std::size_t ply,
    std::string event_type,
    std::optional<Player> actor,
    std::optional<GameAuxAction> action,
    std::optional<GameAuxResult> result,
    std::optional<GameResult> terminal) {
    record_.aux_events.push_back(GameAuxRecord{
        record_.game_id,
        ply,
        next_event_index_++,
        std::move(event_type),
        actor,
        std::move(action),
        std::move(result),
        terminal,
    });
}

void GameRecordRecorder::on_event(const GameEvent& event) {
    switch (event.type) {
        case GameEventType::MoveAccepted:
            if (!event.action) {
                throw std::logic_error("accepted move event is missing action");
            }
            append_aux(
                event.ply,
                "accepted_move",
                event.actor,
                move_action(*event.action),
                status_result("accepted"),
                std::nullopt);
            append_board_state(event.ply);
            break;

        case GameEventType::InvalidMove:
            if (!event.action) {
                throw std::logic_error("invalid move event is missing action");
            }
            append_aux(
                event.ply,
                "illegal_move",
                event.actor,
                move_action(*event.action),
                status_result("illegal", "illegal_move"),
                std::nullopt);
            append_aux(
                event.ply,
                "invalid_move_notification",
                event.actor,
                std::nullopt,
                status_result("notified", "illegal_move"),
                std::nullopt);
            break;

        case GameEventType::Pass:
            append_aux(
                event.ply,
                "pass",
                event.actor,
                pass_action(),
                status_result("accepted"),
                std::nullopt);
            append_board_state(event.ply);
            break;

        case GameEventType::Terminal:
            if (!event.terminal_result) {
                throw std::logic_error("terminal event is missing final result");
            }
            append_aux(
                event.ply,
                "terminal",
                std::nullopt,
                std::nullopt,
                std::nullopt,
                event.terminal_result);
            break;
    }
}

}  // namespace kadoka::othello
