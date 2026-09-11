#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <string>

#include "kadoka_othello/ai.hpp"

namespace kadoka::othello {

struct ObakeMaruConfig {
    double occupied_neighbor_weight{0.7};
    double local_cluster_weight{0.45};
    double mixed_color_weight{0.35};
    double center_bias_weight{0.2};
    double exploration_floor{0.25};
    double randomizer_temperature{4.0};
    double rejected_retry_penalty{0.02};
};

class ObakeMaruAI final : public IAIEngine {
public:
    explicit ObakeMaruAI(
        std::uint64_t seed = 0,
        ObakeMaruConfig config = {});

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] AIOutput think(const AdaptedAIInput& input) override;
    [[nodiscard]] AIInspection inspect(const AdaptedAIInput& input) override;

private:
    struct Candidate {
        Position move{};
        double score{};
        double weight{};
    };

    [[nodiscard]] std::size_t collect_candidates(
        const Board& board,
        std::array<Candidate, 100>& candidates) const;
    [[nodiscard]] double evaluate_position(const Board& board, Position move) const;
    [[nodiscard]] double to_weight(double score, Position move) const noexcept;
    [[nodiscard]] std::uint64_t hash_board(const Board& board) const noexcept;
    void infer_last_result(std::uint64_t current_hash) noexcept;
    [[nodiscard]] Position choose(
        const std::array<Candidate, 100>& candidates,
        std::size_t count);
    void remember(Position move, std::uint64_t board_hash) noexcept;

    ObakeMaruConfig config_;
    std::mt19937_64 rng_;
    Position last_move_{};
    std::uint64_t last_board_hash_{};
    bool has_memory_{};
    bool last_rejected_{};
};

[[nodiscard]] ObakeMaruConfig load_obake_maru_config(const std::string& path);

}  // namespace kadoka::othello
