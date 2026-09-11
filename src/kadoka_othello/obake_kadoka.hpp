#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>

#include "kadoka_othello/ai.hpp"

namespace kadoka::othello {

struct ObakeKadokaConfig {
    double occupied_neighbor_score{0.9};
    double empty_neighbor_penalty{0.2};
    double mixed_color_score{1.4};
    double color_transition_score{1.0};
    double line_interest_score{1.5};
    double local_density_score{0.55};
    double center_early_score{0.25};
    double recent_retry_penalty{0.08};
    double inferred_illegal_retry_penalty{0.01};
    double exploration_floor{0.04};
    double randomizer_temperature{2.4};
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

    struct AttemptMemory {
        Position move{};
        std::uint64_t board_hash{};
        bool inferred_rejected{};
        bool valid{};
    };

    [[nodiscard]] std::size_t collect_candidates(
        const Board& board,
        std::array<WeightedMove, 100>& candidates,
        std::size_t empty_cells) const;
    [[nodiscard]] double evaluate_position(
        const Board& board,
        Position move,
        std::size_t empty_cells) const;
    [[nodiscard]] double score_to_weight(
        double score,
        Position move) const noexcept;
    [[nodiscard]] std::uint64_t hash_board(const Board& board) const noexcept;
    void infer_previous_attempt_result(std::uint64_t current_hash) noexcept;
    [[nodiscard]] const AttemptMemory* find_recent(Position move) const noexcept;
    [[nodiscard]] Position choose_weighted(
        const std::array<WeightedMove, 100>& candidates,
        std::size_t count);
    void remember(Position move, std::uint64_t board_hash) noexcept;

    ObakeKadokaConfig config_;
    std::mt19937_64 rng_;
    std::array<AttemptMemory, 2> recent_{};
    std::size_t recent_count_{};
    std::size_t recent_cursor_{};
};

[[nodiscard]] ObakeKadokaConfig load_obake_kadoka_config(const std::string& path);

}  // namespace kadoka::othello
