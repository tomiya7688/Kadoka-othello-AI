#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>

#include "kadoka_othello/ai.hpp"

namespace kadoka::othello {

struct ObakeKadokaConfig {
    double corner_weight{5.0};
    double edge_weight{2.0};
    double near_piece_weight{1.2};
    double center_weight{0.4};
    double recent_retry_penalty{0.05};
    double exploration_floor{0.15};
    std::size_t memory_depth{2};
};

class ObakeKadokaAI final : public IAIEngine {
public:
    explicit ObakeKadokaAI(
        std::uint64_t seed = 0,
        ObakeKadokaConfig config = {});

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] AIOutput think(const AdaptedAIInput& input) override;
    [[nodiscard]] AIInspection inspect(const AdaptedAIInput& input) override;

private:
    struct WeightedMove {
        Position move{};
        double weight{};
    };

    [[nodiscard]] std::size_t collect_candidates(
        const Board& board,
        std::array<WeightedMove, 100>& candidates) const;
    [[nodiscard]] double score_position(const Board& board, Position move) const;
    [[nodiscard]] bool is_recent(Position move) const noexcept;
    [[nodiscard]] Position choose_weighted(
        const std::array<WeightedMove, 100>& candidates,
        std::size_t count);
    void remember(Position move) noexcept;

    ObakeKadokaConfig config_;
    std::mt19937_64 rng_;
    std::array<Position, 2> recent_{};
    std::size_t recent_count_{};
    std::size_t recent_cursor_{};
};

[[nodiscard]] ObakeKadokaConfig load_obake_kadoka_config(const std::string& path);

}  // namespace kadoka::othello
