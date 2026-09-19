#pragma once

#include <cstddef>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "kadoka_othello/core_state.hpp"
#include "kadoka_othello/game.hpp"

namespace kadoka::othello {

inline constexpr const char* kBoardStateRecordSchema = "kadoka.board_state";
inline constexpr const char* kGameAuxRecordSchema = "kadoka.game_aux";
inline constexpr std::size_t kGameRecordVersion = 1;

struct BoardStateRecord {
    std::string game_id;
    std::size_t ply{};
    CoreState state;
};

enum class GameAuxActionType {
    Move,
    Pass,
};

struct GameAuxAction {
    GameAuxActionType type{GameAuxActionType::Move};
    std::optional<Position> move;
};

struct GameAuxResult {
    std::string status;
    std::optional<std::string> reason;
};

struct GameAuxRecord {
    std::string game_id;
    std::size_t ply{};
    std::size_t event_index{};
    std::string event_type;
    std::optional<Player> actor;
    std::optional<GameAuxAction> action;
    std::optional<GameAuxResult> result;
    std::optional<GameResult> terminal;
};

struct GameRecord {
    std::string game_id;
    std::vector<BoardStateRecord> board_states;
    std::vector<GameAuxRecord> aux_events;
};

using CoreTimeProvider = std::function<CoreTimeState()>;

[[nodiscard]] std::string generate_game_ulid();

[[nodiscard]] std::string board_state_record_to_json(
    const BoardStateRecord& record);
[[nodiscard]] std::string game_aux_record_to_json(
    const GameAuxRecord& record);

[[nodiscard]] BoardStateRecord parse_board_state_record_json(
    const std::string& json);
[[nodiscard]] GameAuxRecord parse_game_aux_record_json(
    const std::string& json);

void write_game_record_jsonl(
    const GameRecord& record,
    std::ostream& board_state_output,
    std::ostream& game_aux_output);

[[nodiscard]] GameRecord read_game_record_jsonl(
    std::istream& board_state_input,
    std::istream& game_aux_input);

class GameRecordRecorder {
public:
    explicit GameRecordRecorder(
        Game& game,
        std::string game_id = {},
        CoreTimeProvider time_provider = {});
    ~GameRecordRecorder();

    GameRecordRecorder(const GameRecordRecorder&) = delete;
    GameRecordRecorder& operator=(const GameRecordRecorder&) = delete;
    GameRecordRecorder(GameRecordRecorder&&) = delete;
    GameRecordRecorder& operator=(GameRecordRecorder&&) = delete;

    [[nodiscard]] const GameRecord& record() const noexcept;

private:
    [[nodiscard]] CoreTimeState current_time() const;
    void append_board_state(std::size_t ply);
    void on_event(const GameEvent& event);
    void append_aux(
        std::size_t ply,
        std::string event_type,
        std::optional<Player> actor,
        std::optional<GameAuxAction> action,
        std::optional<GameAuxResult> result,
        std::optional<GameResult> terminal);

    Game* game_{nullptr};
    GameEventListenerId listener_id_{};
    CoreTimeProvider time_provider_;
    GameRecord record_;
    std::size_t next_event_index_{};
};

}  // namespace kadoka::othello
