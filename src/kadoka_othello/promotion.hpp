#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "kadoka_othello/league_registry.hpp"
#include "kadoka_othello/league_scheduler.hpp"

namespace kadoka::othello {

inline constexpr const char* kPromotionProfileFormat =
    "kadoka.promotion_profile.v1";
inline constexpr const char* kModelGenerationFormat =
    "kadoka.model_generation.v1";
inline constexpr const char* kPromotionDecisionFormat =
    "kadoka.promotion_decision.v1";

struct PromotionThresholds {
    std::size_t min_games_per_board{20};
    double min_score_rate{0.55};
    double min_confidence_lower_bound{0.50};

    std::optional<double> min_rating_delta;
    std::optional<double> min_average_disc_difference;
    std::optional<double> max_average_think_us;
    std::optional<double> min_nodes_per_second;
    std::optional<double> min_simulations_per_second;

    std::optional<double> min_opening_out_score;
    std::optional<double> min_difficult_midgame_score;
    std::optional<double> min_exact_endgame_accuracy;
    std::optional<double> max_major_blunder_rate;

    std::optional<std::uint64_t> max_model_size_bytes;
    std::optional<std::uint64_t> max_peak_memory_bytes;

    bool require_reproducible{true};
};

struct PromotionProfile {
    std::string profile_id;
    std::vector<std::size_t> board_sizes;
    std::size_t games_per_color{10};
    double confidence_z{1.96};
    PromotionThresholds thresholds;

    // Names only. Training/Auto-Tuning implementations remain pluggable.
    std::vector<std::string> tuning_targets;
};

struct CandidateReproducibility {
    std::string parent_model_id;
    std::string parent_model_version;
    std::string training_recipe_id;
    std::vector<std::pair<std::string, std::string>> dataset_versions;
    std::string config_hash;
    std::uint64_t seed{};
    std::string code_version;
    std::string benchmark_result;
    std::vector<std::pair<std::string, std::string>> tuning_parameters;
    std::uint64_t model_size_bytes{};
    std::uint64_t peak_memory_bytes{};
    bool reproducible{false};
};

enum class ModelGenerationStatus {
    Champion,
    Candidate,
    Rejected,
    Retired,
};

struct ModelGeneration {
    std::string generation_id;
    std::string profile_id;
    std::vector<std::string> participant_ids;
    ModelGenerationStatus status{ModelGenerationStatus::Candidate};
    CandidateReproducibility reproducibility;
};

struct PromotionSupplementalMetrics {
    std::optional<double> opening_out_score;
    std::optional<double> difficult_midgame_score;
    std::optional<double> exact_endgame_accuracy;
    std::optional<double> major_blunder_rate;
};

struct PromotionBoardMetrics {
    std::size_t board_size{};
    std::size_t games{};
    double score_rate{};
    double confidence_lower_bound{};
    double confidence_upper_bound{};
    std::size_t candidate_black_games{};
    double candidate_black_score_rate{};
    std::size_t candidate_white_games{};
    double candidate_white_score_rate{};
    double average_disc_difference{};
    int min_disc_difference{};
    int max_disc_difference{};
    double average_think_us{};
    std::optional<double> nodes_per_second;
    std::optional<double> simulations_per_second;
    double candidate_rating{};
    double champion_rating{};
    double rating_delta{};
};

struct PromotionEvaluation {
    std::string profile_id;
    std::string candidate_generation_id;
    std::string champion_generation_id;
    std::vector<PromotionBoardMetrics> board_metrics;
    PromotionSupplementalMetrics supplemental;
    std::vector<std::string> game_ids;
};

struct PromotionDecision {
    std::string decision_id;
    bool promoted{false};
    std::vector<std::string> reasons;
    PromotionEvaluation evaluation;
};

class ChampionRegistry {
public:
    void register_profile(PromotionProfile profile);
    void bootstrap_champion(
        ModelGeneration generation,
        LeagueRegistry& league_registry);
    void register_candidate(
        ModelGeneration generation,
        LeagueRegistry& league_registry);

    [[nodiscard]] const PromotionProfile* find_profile(
        const std::string& profile_id) const noexcept;
    [[nodiscard]] PromotionProfile* find_profile(
        const std::string& profile_id) noexcept;

    [[nodiscard]] const ModelGeneration* find_generation(
        const std::string& generation_id) const noexcept;
    [[nodiscard]] ModelGeneration* find_generation(
        const std::string& generation_id) noexcept;

    [[nodiscard]] const ModelGeneration* current_champion(
        const std::string& profile_id) const noexcept;

    [[nodiscard]] const std::vector<PromotionProfile>& profiles()
        const noexcept;
    [[nodiscard]] const std::vector<ModelGeneration>& generations()
        const noexcept;

    void apply_decision(
        const PromotionDecision& decision,
        LeagueRegistry& league_registry);

private:
    std::vector<PromotionProfile> profiles_;
    std::vector<ModelGeneration> generations_;
};

[[nodiscard]] const char* model_generation_status_name(
    ModelGenerationStatus status) noexcept;
[[nodiscard]] ModelGenerationStatus parse_model_generation_status(
    std::string_view value);

[[nodiscard]] std::string generate_generation_id();
[[nodiscard]] std::string generate_promotion_decision_id();

[[nodiscard]] PromotionEvaluation evaluate_candidate_generation(
    LeagueRegistry& league_registry,
    const PromotionProfile& profile,
    const ModelGeneration& champion,
    const ModelGeneration& candidate,
    const PromotionSupplementalMetrics& supplemental,
    std::uint64_t evaluation_seed,
    const LeagueRunOutputs& outputs = {});

[[nodiscard]] PromotionDecision decide_promotion(
    const PromotionProfile& profile,
    const ModelGeneration& candidate,
    const PromotionEvaluation& evaluation);

[[nodiscard]] std::string promotion_profile_to_json(
    const PromotionProfile& profile);
[[nodiscard]] std::string model_generation_to_json(
    const ModelGeneration& generation);
[[nodiscard]] std::string promotion_decision_to_json(
    const PromotionDecision& decision);

void write_champion_registry_jsonl(
    const ChampionRegistry& registry,
    std::ostream& output);

void write_promotion_decision_jsonl(
    const PromotionDecision& decision,
    std::ostream& output);

}  // namespace kadoka::othello
