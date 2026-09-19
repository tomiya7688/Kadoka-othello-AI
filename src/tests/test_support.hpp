#pragma once

#include <stdexcept>
#include <string>

namespace kadoka::othello::test {

inline void require(
    bool condition,
    const char* expression,
    const char* file,
    int line) {
    if (condition) return;
    throw std::runtime_error(
        std::string(file) + ":" + std::to_string(line) +
        " test requirement failed: " + expression);
}

}  // namespace kadoka::othello::test

#define KADOKA_REQUIRE(expression) \
    ::kadoka::othello::test::require( \
        static_cast<bool>(expression), #expression, __FILE__, __LINE__)
