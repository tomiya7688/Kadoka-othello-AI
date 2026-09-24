#include <cmath>
#include <string>
#include <vector>

#include "kadoka_othello/confidence.hpp"
#include "test_support.hpp"

using namespace kadoka::othello;

namespace {

ConfidenceSample make_high_confidence_sample() {
    ConfidenceSample sample;
    sample.game_id = "game-high";
    sample.ply = 30;
    sample.board_size = 8;
    sample.factors.set(
        confidence_factor::kSourceRatingDeviation, 0.0);
    sample.factors.set(confidence_factor::kSearchDepth, 30.0);
    sample.factors.set(confidence_factor::kNodes, 500000.0);
    sample.factors.set(
        confidence_factor::kSimulations, 50000.0);
    sample.factors.set(
        confidence_factor::kTopCandidateGap, 3.0);
    sample.factors.set(
        confidence_factor::kPolicyEntropy, 0.0);
    sample.factors.set(
        confidence_factor::kMultiAiMoveDisagreement, 0.0);
    sample.factors.set(
        confidence_factor::kMultiAiValueDisagreement, 0.0);
    sample.factors.set(
        confidence_factor::kMonteCarloVariance, 0.0);
    sample.factors.set(
        confidence_factor::kExactEndgameAgreement, 1.0);
    sample.factors.set(
        confidence_factor::kResultConsistency, 1.0);
    return sample;
}

ConfidenceSample make_low_confidence_sample() {
    ConfidenceSample sample;
    sample.game_id = "game-low";
    sample.ply = 30;
    sample.board_size = 8;
    sample.factors.set(
        confidence_factor::kSourceRatingDeviation, 350.0);
    sample.factors.set(confidence_factor::kSearchDepth, 0.0);
    sample.factors.set(confidence_factor::kNodes, 0.0);
    sample.factors.set(
        confidence_factor::kSimulations, 0.0);
    sample.factors.set(
        confidence_factor::kTopCandidateGap, 0.0);
    sample.factors.set(
        confidence_factor::kPolicyEntropy, 1.0);
    sample.factors.set(
        confidence_factor::kMultiAiMoveDisagreement, 1.0);
    sample.factors.set(
        confidence_factor::kMultiAiValueDisagreement, 1.0);
    sample.factors.set(
        confidence_factor::kEvalModelDisagreement, 1.0);
    sample.factors.set(
        confidence_factor::kMonteCarloVariance, 1.0);
    sample.factors.set(
        confidence_factor::kResultConsistency, 0.0);
    return sample;
}

void test_factor_round_trip() {
    ConfidenceSample sample;
    sample.game_id = "01ARZ3NDEKTSV4RRFFQ69G5FAV";
    sample.ply = 21;
    sample.board_size = 10;
    sample.factors.set(confidence_factor::kNodes, 12345.0);
    sample.factors.set(
        confidence_factor::kMonteCarloVariance, 0.125);
    sample.factors.set("future_custom_factor", 7.5);

    const std::string json = confidence_sample_to_json(sample);
    const ConfidenceSample parsed =
        parse_confidence_sample_json(json);

    KADOKA_REQUIRE(parsed.game_id == sample.game_id);
    KADOKA_REQUIRE(parsed.ply == 21);
    KADOKA_REQUIRE(parsed.board_size == 10);
    KADOKA_REQUIRE(
        parsed.factors.get(confidence_factor::kNodes) ==
        std::optional<double>{12345.0});
    KADOKA_REQUIRE(
        parsed.factors.get("future_custom_factor") ==
        std::optional<double>{7.5});
}

void test_statistical_helpers() {
    const double uniform_entropy =
        normalized_policy_entropy({1.0, 1.0, 1.0, 1.0});
    KADOKA_REQUIRE(uniform_entropy > 0.999);

    const double certain_entropy =
        normalized_policy_entropy({1.0, 0.0, 0.0, 0.0});
    KADOKA_REQUIRE(certain_entropy < 0.001);

    const double move_split = move_disagreement({
        Position{2, 3},
        Position{2, 3},
        Position{3, 2},
        Position{4, 5},
    });
    KADOKA_REQUIRE(
        move_split > 0.49 && move_split < 0.51);

    const double value_split =
        value_disagreement_stddev({1.0, 2.0, 3.0});
    KADOKA_REQUIRE(value_split > 0.8);

    const double variance = sample_variance({1.0, 2.0, 3.0});
    KADOKA_REQUIRE(
        variance > 0.999 && variance < 1.001);
}

void test_baseline_confidence_and_exact_endgame() {
    BaselineConfidenceCalculator calculator;
    const auto parameters = default_confidence_board_parameters();

    const auto& eight =
        find_confidence_board_parameters(parameters, 8);
    const ConfidenceEstimate high =
        calculator.evaluate(
            make_high_confidence_sample(),
            eight);
    const ConfidenceEstimate low =
        calculator.evaluate(
            make_low_confidence_sample(),
            eight);

    KADOKA_REQUIRE(high.confidence > 0.9);
    KADOKA_REQUIRE(low.confidence < 0.2);
    KADOKA_REQUIRE(
        low.reanalysis_priority >
        high.reanalysis_priority);

    ConfidenceSample endgame;
    endgame.game_id = "game-end";
    endgame.ply = 50;
    endgame.board_size = 8;
    endgame.factors.set(confidence_factor::kEmptyCount, 10.0);
    endgame.factors.set(
        confidence_factor::kExactEndgameAvailable, 1.0);
    const ConfidenceEstimate exact =
        calculator.evaluate(endgame, eight);
    KADOKA_REQUIRE(exact.exact_endgame_candidate);

    ConfidenceSample ten_game = endgame;
    ten_game.game_id = "game-ten";
    ten_game.board_size = 10;
    ten_game.factors.set(
        confidence_factor::kExactEndgameAvailable, 0.0);
    ten_game.factors.set(confidence_factor::kEmptyCount, 11.0);
    const auto& ten =
        find_confidence_board_parameters(parameters, 10);
    const ConfidenceEstimate ten_estimate =
        calculator.evaluate(ten_game, ten);
    KADOKA_REQUIRE(!ten_estimate.exact_endgame_candidate);
}

void test_monte_carlo_and_exact_agreement_affect_priority() {
    BaselineConfidenceCalculator calculator;
    const auto parameters = default_confidence_board_parameters();
    const auto& eight =
        find_confidence_board_parameters(parameters, 8);

    ConfidenceSample stable = make_high_confidence_sample();
    stable.game_id = "stable";
    stable.factors.set(
        confidence_factor::kMonteCarloVariance, 0.0);
    stable.factors.set(
        confidence_factor::kExactEndgameAgreement, 1.0);

    ConfidenceSample unstable = stable;
    unstable.game_id = "unstable";
    unstable.factors.set(
        confidence_factor::kMonteCarloVariance, 1.0);
    unstable.factors.set(
        confidence_factor::kExactEndgameAgreement, 0.0);

    const auto stable_estimate =
        calculator.evaluate(stable, eight);
    const auto unstable_estimate =
        calculator.evaluate(unstable, eight);

    KADOKA_REQUIRE(
        unstable_estimate.confidence <
        stable_estimate.confidence);
    KADOKA_REQUIRE(
        unstable_estimate.reanalysis_priority >
        stable_estimate.reanalysis_priority);
}

void test_priority_sampler_and_high_confidence_audit() {
    BaselineConfidenceCalculator calculator;
    auto parameters = default_confidence_board_parameters();
    for (auto& item : parameters) {
        item.high_confidence_threshold = 0.8;
        item.high_confidence_audit_rate = 1.0;
    }

    ConfidenceSample high = make_high_confidence_sample();
    ConfidenceSample low = make_low_confidence_sample();

    ConfidenceSample medium;
    medium.game_id = "game-medium";
    medium.ply = 30;
    medium.board_size = 8;
    medium.factors.set(
        confidence_factor::kSourceRatingDeviation, 150.0);
    medium.factors.set(
        confidence_factor::kTopCandidateGap, 0.5);

    const std::vector<ConfidenceSample> samples{
        high,
        low,
        medium,
    };
    const auto selected = select_reanalysis_samples(
        samples,
        calculator,
        parameters,
        2,
        12345);

    KADOKA_REQUIRE(selected.size() == 2);

    bool found_high_audit = false;
    bool found_low_priority = false;
    for (const auto& item : selected) {
        if (item.sample_index == 0 && item.random_audit) {
            found_high_audit = true;
        }
        if (item.sample_index == 1 && !item.random_audit) {
            found_low_priority = true;
        }
    }
    KADOKA_REQUIRE(found_high_audit);
    KADOKA_REQUIRE(found_low_priority);
}

void test_board_size_parameters() {
    const auto parameters = default_confidence_board_parameters();
    KADOKA_REQUIRE(
        find_confidence_board_parameters(parameters, 6).
            exact_endgame_empty_threshold == 12);
    KADOKA_REQUIRE(
        find_confidence_board_parameters(parameters, 8).
            exact_endgame_empty_threshold == 14);
    KADOKA_REQUIRE(
        find_confidence_board_parameters(parameters, 10).
            exact_endgame_empty_threshold == 10);
}

}  // namespace

int main() {
    test_factor_round_trip();
    test_statistical_helpers();
    test_baseline_confidence_and_exact_endgame();
    test_monte_carlo_and_exact_agreement_affect_priority();
    test_priority_sampler_and_high_confidence_audit();
    test_board_size_parameters();
    return 0;
}
