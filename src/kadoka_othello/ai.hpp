#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "kadoka_othello/board.hpp"

namespace kadoka::othello {

struct AIInput {
    const Board* board{nullptr};
    const std::vector<Position>* legal_moves{nullptr};
};

struct AIOutput {
    Position move{};
};

struct AdaptedAIInput {
    const Board* board{nullptr};
    const std::vector<Position>* legal_moves{nullptr};
};

class IAIAdapter {
public:
    virtual ~IAIAdapter() = default;

    [[nodiscard]] virtual AdaptedAIInput adapt(const AIInput& input) const = 0;
};

class PassThroughAdapter final : public IAIAdapter {
public:
    [[nodiscard]] AdaptedAIInput adapt(const AIInput& input) const override;
};

class DropLegalMovesAdapter final : public IAIAdapter {
public:
    [[nodiscard]] AdaptedAIInput adapt(const AIInput& input) const override;
};

class IAIEngine {
public:
    virtual ~IAIEngine() = default;

    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual AIOutput think(const AdaptedAIInput& input) = 0;
};

class RandomAI final : public IAIEngine {
public:
    explicit RandomAI(std::uint64_t seed = 0);

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] AIOutput think(const AdaptedAIInput& input) override;

private:
    std::uint64_t seed_;
};

struct AIPackage {
    IAIEngine* engine{nullptr};
    const IAIAdapter* adapter{nullptr};
};

[[nodiscard]] AIOutput invoke_ai(
    AIPackage package,
    const AIInput& input);

}  // namespace kadoka::othello
