#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
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

struct AICandidate {
    Position move{};
    double value{};
    double policy{};
};

struct AIDiagnostic {
    std::string key;
    std::string value;
};

struct AIInspection {
    AIOutput output{};
    std::vector<AICandidate> candidates;
    std::vector<AIDiagnostic> diagnostics;
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

    // Development tools may request candidates and diagnostics. Games should use think() only.
    [[nodiscard]] virtual AIInspection inspect(const AdaptedAIInput& input);
};

class RandomAI final : public IAIEngine {
public:
    explicit RandomAI(std::uint64_t seed = 0);

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] AIOutput think(const AdaptedAIInput& input) override;
    [[nodiscard]] AIInspection inspect(const AdaptedAIInput& input) override;

private:
    std::mt19937_64 rng_;
};

struct AIPackage {
    IAIEngine* engine{nullptr};
    const IAIAdapter* adapter{nullptr};
};

[[nodiscard]] AIOutput invoke_ai(
    AIPackage package,
    const AIInput& input);

[[nodiscard]] AIInspection inspect_ai(
    AIPackage package,
    const AIInput& input);

}  // namespace kadoka::othello
