#pragma once

#include <cstddef>
#include <functional>
#include <optional>
#include <vector>

#include "kadoka_othello/rules.hpp"

namespace kadoka::othello {

enum class GameStatus { Playing, Finished };
enum class Winner { Black, White, Draw };

struct GameResult {
    std::size_t black_discs{};
    std::size_t white_discs{};
    Winner winner{Winner::Draw};
};

enum class GameEventType {
    MoveAccepted,
    InvalidMove,
    Pass,
    Terminal,
};

struct GameEvent {
    GameEventType type{GameEventType::InvalidMove};
    std::size_t ply{};
    Player actor{Player::Black};
    std::optional<Position> action;
    std::optional<GameResult> terminal_result;
};

using GameEventListener = std::function<void(const GameEvent&)>;

class Game {
public:
    explicit Game(std::size_t board_size = 8);
    [[nodiscard]] const Board& board() const noexcept;
    [[nodiscard]] Player current_player() const noexcept;
    [[nodiscard]] GameStatus status() const noexcept;
    [[nodiscard]] std::size_t ply() const noexcept;
    [[nodiscard]] const std::vector<Move>& history() const noexcept;
    [[nodiscard]] std::vector<Position> legal_moves() const;
    [[nodiscard]] bool can_pass() const;
    [[nodiscard]] std::optional<GameResult> result() const;
    bool play(Position position);
    bool pass();
    void reset();
    void add_event_listener(GameEventListener listener);

private:
    void advance_turn();
    void update_finished_state();
    void emit_event(const GameEvent& event) const;
    void emit_terminal_if_finished(Player actor) const;

    Board board_;
    Player current_player_{Player::Black};
    GameStatus status_{GameStatus::Playing};
    std::size_t consecutive_passes_{0};
    std::size_t ply_{0};
    std::vector<Move> history_;
    std::vector<GameEventListener> listeners_;
};

}  // namespace kadoka::othello
