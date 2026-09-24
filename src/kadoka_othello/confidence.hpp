#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "kadoka_othello/types.hpp"

namespace kadoka::othello {

inline constexpr const char* kConfidenceSampleFormat =
    "kadoka.confidence_sample.v1";

namespace confidence_factor {
inline constexpr const char* kSourceRating = "source_rating";
inline constexpr const char* kSourceRatingDeviation =
    "source_rating_deviation";
inline constexpr const char* kSearchDepth = "search_depth";
inline constexpr const char* kSimulations = "simulations";
inline constexpr const char* kNodes = "nodes";
inline constexpr const char* kSearchTimeMs = "search_time_ms";
inline constexpr const char* kTopCandidateGap = "top_candidate_gap";
inline constexpr const char* kPolicyEntropy = "policy_entropy";
inline constexpr const char* kMultiAiMoveDisagreement =
    "multi_ai_move_disagreement";
inline constexpr const char* kMultiAiValueDisagreement =
    "multi_ai_value_disagreement";
inline constexpr const char* kEvalModelDisagreement =
    "eval_model_disagreement";
inline constexpr const char* kMonteCarloVariance =
    "monte_carlo_variance";
inline constexpr const char* kLegalMoveCount = "legal_move_count";
inline constexpr const char* kMobilitySelf = "mobility_self";
inline constexpr const char* kMobilityOpponent =
    "mobility_opponent";
inline constexpr const char* kPotentialMobilitySelf =
    "potential_mobility_self";
inline constexpr const char* kPotentialMobilityOpponent =
    "potential_mobility_opponent";
inline constexpr const char* kFrontierSelf = "frontier_self";
inline constexpr const char* kFrontierOpponent =
    "frontier_opponent";
inline constexpr const char* kCornerRisk = "corner_risk";
inline constexpr const char* kXSquareRisk = "x_square_risk";
inline constexpr const char* kCSquareRisk = "c_square_risk";
inline constexpr const char* kParityDamage = "parity_damage";
inline constexpr const char* kRegionParityUncertainty =
    "region_parity_uncertainty";
inline constexpr const char* kStableDiscsSelf =
    "stable_discs_self";
inline constexpr const char* kStableDiscsOpponent =
    "stable_discs_opponent";
inline constexpr const char* kEmptyCount = "empty_count";
inline constexpr const char* kExactEndgameAvailable =
    "exact_endgame_available";
inline constexpr const char* kExactEndgameAgreement =
    "exact_endgame_agreement";
inline constexpr const char* kResultConsistency =
    "result_consistency";
}  // namespace confidence_factor

class ConfidenceFactors {
public:
    void set(std::string key, double value);
    [[nodiscard]] std::optional<double> get(
        std::string_view key) const noexcept;
    [[nodiscard]] const std::vector<std::pair<std::string, double>>&
    entries() const noexcept;

private:
    std::vector<std::pair<std::string, double>> entries_;
};

struct ConfidenceSample {
    std::string game_id;
    std::size_t ply{};
    std::size_t board_size{8};
    ConfidenceFactors factors;
};

struct ConfidenceBoardParameters {
    std::size_t board_size{8};
    std::size_t exact_endgame_empty_threshold{12};
    std::size_t midgame_ply_begin{12};
    std::size_t midgame_ply_end{44};
    double high_confidence_threshold{0.85};
    double high_confidence_audit_rate{0.05};

    // Baseline calculator scales. Different calculators may ignore them.
    double depth_scale{12.0};
    double nodes_scale{100000.0};
    double simulations_scale{10000.0};
    double candidate_gap_scale{1.0};
    double value_disagreement_scale{1.0};
    double eval_disagreement_scale{1.0};
    double monte_carlo_variance_scale{1.0};
};

struct ConfidenceEstimate {
    double confidence{};
    double uncertainty{1.0};
    double reanalysis_priority{1.0};
    bool exact_endgame_candidate{false};
};

class IConfidenceCalculator {
public:
    virtual ~IConfidenceCalculator() = default;

    [[nodiscard]] virtual std::string id() const = 0;
    [[nodiscard]] virtual ConfidenceEstimate evaluate(
        const ConfidenceSample& sample,
        const ConfidenceBoardParameters& parameters) const = 0;
};

class BaselineConfidenceCalculator final
    : public IConfidenceCalculator {
public:
    [[nodiscard]] std::string id() const override;
    [[nodiscard]] ConfidenceEstimate evaluate(
        const ConfidenceSample& sample,
        const ConfidenceBoardParameters& parameters) const override;
};

struct ReanalysisSelection {
    std::size_t sample_index{};
    ConfidenceEstimate estimate;
    bool random_audit{false};
};

[[nodiscard]] const ConfidenceBoardParameters&
find_confidence_board_parameters(
    const std::vector<ConfidenceBoardParameters>& parameters,
    std::size_t board_size);

[[nodiscard]] std::vector<ConfidenceBoardParameters>
default_confidence_board_parameters();

[[nodiscard]] std::vector<ReanalysisSelection>
select_reanalysis_samples(
    const std::vector<ConfidenceSample>& samples,
    const IConfidenceCalculator& calculator,
    const std::vector<ConfidenceBoardParameters>& parameters,
    std::size_t max_samples,
    std::uint64_t seed);

[[nodiscard]] double normalized_policy_entropy(
    const std::vector<double>& policy);

[[nodiscard]] double move_disagreement(
    const std::vector<Position>& moves);

[[nodiscard]] double value_disagreement_stddev(
    const std::vector<double>& values);

[[nodiscard]] double sample_variance(
    const std::vector<double>& values);

[[nodiscard]] std::string confidence_sample_to_json(
    const ConfidenceSample& sample);

[[nodiscard]] ConfidenceSample parse_confidence_sample_json(
    std::string_view json);

}  // namespace kadoka::othello
