#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <string_view>
#include <vector>

#include "kadoka_othello/core_state.hpp"

namespace kadoka::othello {

using AIInput = CoreStateView;

struct AIMoveMetrics {
    std::optional<std::uint64_t> nodes;
    std::optional<std::uint64_t> simulations;
    std::optional<std::size_t> depth;
    std::optional<double> search_effort;
};

struct AIOutput {
    Position move{};
    AIMoveMetrics metrics;
};

void apply_standard_ai_metric(
    AIOutput& output,
    std::string_view key,
    std::string_view value);

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
