#pragma once

#include <cstddef>
#include <vector>

#include "kadoka_othello/types.hpp"

namespace kadoka::othello {

class Board {
public:
    explicit Board(std::size_t size = 8);

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool in_bounds(Position position) const noexcept;
    [[nodiscard]] Cell at(Position position) const;
    [[nodiscard]] std::size_t count(Cell cell) const noexcept;
    [[nodiscard]] bool full() const noexcept;

    void set(Position position, Cell cell);
    void reset();

private:
    [[nodiscard]] std::size_t index(Position position) const;
    void validate_size(std::size_t size) const;

    std::size_t size_;
    std::vector<Cell> cells_;
};

}  // namespace kadoka::othello
