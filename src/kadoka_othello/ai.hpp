#pragma once

#include <cstddef>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#include "kadoka_othello/core_state.hpp"

namespace kadoka::othello {

using AIInput = CoreStateView;

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

class IAIEngine {
public:
    virtual ~IAIEngine() = default;

    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual AIOutput think(const AIInput& input) = 0;

    // Development tools may request candidates and diagnostics. Games should use think() only.
    [[nodiscard]] virtual AIInspection inspect(const AIInput& input);
};

class RandomAI final : public IAIEngine {
public:
    explicit RandomAI(std::uint64_t seed = 0);

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] AIOutput think(const AIInput& input) override;
    [[nodiscard]] AIInspection inspect(const AIInput& input) override;

private:
    std::mt19937_64 rng_;
};

struct AIPackage {
    IAIEngine* engine{nullptr};
};

[[nodiscard]] AIOutput invoke_ai(
    AIPackage package,
    const AIInput& input);

[[nodiscard]] AIInspection inspect_ai(
    AIPackage package,
    const AIInput& input);

}  // namespace kadoka::othello
