#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>

#include "kadoka_othello/ai.hpp"

namespace kadoka::othello {

struct ObakeKadokaConfig {
    double corner_score{8.0};
    double edge_score{2.0};
    double x_square_penalty{7.0};
    double c_square_penalty{4.0};
    double occupied_neighbor_score{0.7};
    double empty_neighbor_penalty{0.25};
    double mixed_color_score{1.2};
    double line_potential_score{1.6};
    double center_early_score{0.5};
    double recent_retry_penalty{0.05};
    double exploration_floor{0.02};
    double randomizer_temperature{3.0};
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
        double score{};
        double weight{};
    };

    [[nodiscard]] std::size_t collect_candidates(
        const Board& board,
        std::array<WeightedMove, 100>& candidates) const;
    [[nodiscard]] double evaluate_position(
        const Board& board,
        Position move,
        std::size_t empty_cells) const;
    [[nodiscard]] double score_to_weight(double score, bool recent) const noexcept;
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
