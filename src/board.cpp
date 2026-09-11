#include "kadoka_othello/board.hpp"

#include <algorithm>
#include <stdexcept>

namespace kadoka::othello {

Board::Board(std::size_t size)
    : size_(size), cells_(size * size, Cell::Empty) {
    validate_size(size_);
    reset();
}

std::size_t Board::size() const noexcept {
    return size_;
}

bool Board::in_bounds(Position position) const noexcept {
    return position.row < size_ && position.col < size_;
}

Cell Board::at(Position position) const {
    return cells_.at(index(position));
}

std::size_t Board::count(Cell cell) const noexcept {
    return static_cast<std::size_t>(std::count(cells_.begin(), cells_.end(), cell));
}

bool Board::full() const noexcept {
    return std::none_of(cells_.begin(), cells_.end(), [](Cell cell) {
        return cell == Cell::Empty;
    });
}

void Board::set(Position position, Cell cell) {
    cells_.at(index(position)) = cell;
}

void Board::reset() {
    std::fill(cells_.begin(), cells_.end(), Cell::Empty);

    const std::size_t upper = (size_ / 2) - 1;
    const std::size_t lower = size_ / 2;

    set({upper, upper}, Cell::White);
    set({upper, lower}, Cell::Black);
    set({lower, upper}, Cell::Black);
    set({lower, lower}, Cell::White);
}

std::size_t Board::index(Position position) const {
    if (!in_bounds(position)) {
        throw std::out_of_range("board position is out of range");
    }
    return position.row * size_ + position.col;
}

void Board::validate_size(std::size_t size) const {
    if (size < 4 || size % 2 != 0) {
        throw std::invalid_argument("board size must be an even number greater than or equal to 4");
    }
}

}  // namespace kadoka::othello
