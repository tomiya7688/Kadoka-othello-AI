#pragma once

#include <cstddef>
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

class Game {
public:
    explicit Game(std::size_t board_size = 8);
    [[nodiscard]] const Board& board() const noexcept;
    [[nodiscard]] Player current_player() const noexcept;
    [[nodiscard]] GameStatus status() const noexcept;
    [[nodiscard]] const std::vector<Move>& history() const noexcept;
    [[nodiscard]] std::vector<Position> legal_moves() const;
    [[nodiscard]] bool can_pass() const;
    [[nodiscard]] std::optional<GameResult> result() const;
    bool play(Position position);
    bool pass();
    void reset();
private:
    void advance_turn();
    void update_finished_state();
    Board board_;
    Player current_player_{Player::Black};
    GameStatus status_{GameStatus::Playing};
    std::size_t consecutive_passes_{0};
    std::vector<Move> history_;
};

}  // namespace kadoka::othello
