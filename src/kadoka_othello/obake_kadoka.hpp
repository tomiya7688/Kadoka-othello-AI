#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>

#include "kadoka_othello/ai.hpp"
#include "kadoka_othello/obake_kadoka_evaluator.hpp"

namespace kadoka::othello {

struct ObakeKadokaConfig {
    ObakeKadokaEvaluatorWeights evaluator_weights{};
    double recent_retry_penalty{0.08};
    double inferred_illegal_retry_penalty{0.005};
    double exploration_floor{0.025};
    double randomizer_temperature{1.65};
    std::size_t memory_depth{2};
};

class ObakeKadokaAI final : public IAIEngine {
public:
    explicit ObakeKadokaAI(
        std::uint64_t seed = 0,
        ObakeKadokaConfig config = {});

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] AIOutput think(const AIInput& input) override;
    [[nodiscard]] AIInspection inspect(const AIInput& input) override;

private:
    struct WeightedMove {
        Position move{};
        ObakeKadokaEvaluation evaluation{};
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
    ObakeKadokaEvaluator evaluator_;
    std::mt19937_64 rng_;
    std::array<AttemptMemory, 2> recent_{};
    std::size_t recent_count_{};
    std::size_t recent_cursor_{};
};

// Legacy single-file parameter loader.
[[nodiscard]] ObakeKadokaConfig load_obake_kadoka_config(const std::string& path);

// Standard Kadoka model loader: model.json is a root descriptor that references assets.
[[nodiscard]] ObakeKadokaConfig load_obake_kadoka_model(const std::string& model_root_path);

}  // namespace kadoka::othello
