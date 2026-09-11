#include "kadoka_othello/game.hpp"

namespace kadoka::othello {

Game::Game(std::size_t board_size)
    : board_(board_size) {
    update_finished_state();
}

const Board& Game::board() const noexcept {
    return board_;
}

Player Game::current_player() const noexcept {
    return current_player_;
}

GameStatus Game::status() const noexcept {
    return status_;
}

const std::vector<Move>& Game::history() const noexcept {
    return history_;
}

std::vector<Position> Game::legal_moves() const {
    if (status_ == GameStatus::Finished) {
        return {};
    }
    return rules::legal_moves(board_, current_player_);
}

bool Game::can_pass() const {
    return status_ == GameStatus::Playing &&
           !rules::has_legal_move(board_, current_player_);
}

std::optional<GameResult> Game::result() const {
    if (status_ != GameStatus::Finished) {
        return std::nullopt;
    }

    const std::size_t black = board_.count(Cell::Black);
    const std::size_t white = board_.count(Cell::White);

    Winner winner = Winner::Draw;
    if (black > white) {
        winner = Winner::Black;
    } else if (white > black) {
        winner = Winner::White;
    }

    return GameResult{black, white, winner};
}

bool Game::play(Position position) {
    if (status_ == GameStatus::Finished) {
        return false;
    }

    if (!rules::apply_move(board_, position, current_player_)) {
        return false;
    }

    history_.push_back(Move{current_player_, position});
    consecutive_passes_ = 0;
    advance_turn();
    update_finished_state();
    return true;
}

bool Game::pass() {
    if (!can_pass()) {
        return false;
    }

    ++consecutive_passes_;
    advance_turn();
    update_finished_state();
    return true;
}

void Game::reset() {
    board_.reset();
    current_player_ = Player::Black;
    status_ = GameStatus::Playing;
    consecutive_passes_ = 0;
    history_.clear();
    update_finished_state();
}

void Game::advance_turn() {
    current_player_ = opponent(current_player_);
}

void Game::update_finished_state() {
    if (board_.full() || consecutive_passes_ >= 2) {
        status_ = GameStatus::Finished;
        return;
    }

    const bool current_has_move = rules::has_legal_move(board_, current_player_);
    const bool other_has_move = rules::has_legal_move(board_, opponent(current_player_));
    status_ = (!current_has_move && !other_has_move)
        ? GameStatus::Finished
        : GameStatus::Playing;
}

}  // namespace kadoka::othello
