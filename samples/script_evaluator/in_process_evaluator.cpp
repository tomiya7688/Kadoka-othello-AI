#include "kadoka_othello/script_evaluator_abi.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <string>

#ifdef _WIN32
#define KADOKA_EVALUATOR_EXPORT __declspec(dllexport)
#else
#define KADOKA_EVALUATOR_EXPORT __attribute__((visibility("default")))
#endif

namespace {

bool ends_with(const char* value, const char* suffix) {
    if (value == nullptr || suffix == nullptr)
        return false;
    const std::size_t value_length = std::strlen(value);
    const std::size_t suffix_length = std::strlen(suffix);
    if (suffix_length > value_length)
        return false;
    return std::strcmp(value + value_length - suffix_length, suffix) == 0;
}

double feature_value(const KadokaScriptEvaluatorCaseV1& item, const char* key) {
    for (std::size_t i = 0; i < item.feature_count; ++i) {
        if (std::strcmp(item.features[i].key, key) == 0)
            return item.features[i].value;
    }
    return 0.0;
}

bool has_feature(const KadokaScriptEvaluatorCaseV1& item, const char* key) {
    for (std::size_t i = 0; i < item.feature_count; ++i) {
        if (std::strcmp(item.features[i].key, key) == 0)
            return true;
    }
    return false;
}

double evaluate_score(const KadokaScriptEvaluatorCaseV1& item) {
    if (has_feature(item, "opponent_mobility")) {
        return 100.0 * feature_value(item, "is_corner") + 2.0 * feature_value(item, "is_edge") -
               30.0 * feature_value(item, "opponent_mobility") -
               12.0 * feature_value(item, "adjacent_to_unoccupied_corner") -
               0.25 * feature_value(item, "flipped_disc_count") +
               0.1 * feature_value(item, "disc_difference_after_move");
    }

    double score = 0.0;
    for (std::size_t i = 0; i < item.feature_count; ++i) {
        const auto& feature = item.features[i];
        if (ends_with(feature.key, "bonus"))
            score += feature.value;
        else if (ends_with(feature.key, "penalty"))
            score -= feature.value;
        else
            score += feature.value * 0.1;
    }
    return score;
}

} // namespace

extern "C" KADOKA_EVALUATOR_EXPORT int kadoka_script_evaluator_v1(const KadokaScriptEvaluatorCaseV1* cases,
                                                                  std::size_t case_count,
                                                                  const KadokaScriptEvaluatorSinkV1* sink) {
    if (cases == nullptr || sink == nullptr || sink->context == nullptr)
        return 10;
    if (sink->begin_result == nullptr || sink->value == nullptr || sink->diagnostic == nullptr ||
        sink->end_result == nullptr) {
        return 11;
    }

    for (std::size_t i = 0; i < case_count; ++i) {
        const double score = evaluate_score(cases[i]);
        const double confidence = std::min(1.0, std::abs(score) / 10.0);

        if (sink->begin_result(sink->context, cases[i].id) != 0)
            return 20;
        if (sink->value(sink->context, "score", score) != 0)
            return 21;
        if (sink->value(sink->context, "confidence", confidence) != 0)
            return 22;
        if (sink->diagnostic(sink->context, "runtime", "native_in_process") != 0)
            return 23;
        const std::string flipped_disc_count = std::to_string(feature_value(cases[i], "flipped_disc_count"));
        if (sink->diagnostic(sink->context, "flipped_disc_count", flipped_disc_count.c_str()) != 0)
            return 24;
        const std::string opponent_legal_move_count =
            std::to_string(feature_value(cases[i], "opponent_legal_move_count"));
        if (sink->diagnostic(sink->context, "opponent_legal_move_count", opponent_legal_move_count.c_str()) != 0)
            return 25;
        const std::string opponent_mobility = std::to_string(feature_value(cases[i], "opponent_mobility"));
        if (sink->diagnostic(sink->context, "opponent_mobility", opponent_mobility.c_str()) != 0)
            return 26;
        const std::string disc_difference_after_move =
            std::to_string(feature_value(cases[i], "disc_difference_after_move"));
        if (sink->diagnostic(sink->context, "disc_difference_after_move", disc_difference_after_move.c_str()) != 0)
            return 27;
        const std::string adjacent_to_unoccupied_corner =
            std::to_string(feature_value(cases[i], "adjacent_to_unoccupied_corner"));
        if (sink->diagnostic(sink->context, "adjacent_to_unoccupied_corner", adjacent_to_unoccupied_corner.c_str()) !=
            0)
            return 28;
        if (sink->end_result(sink->context) != 0)
            return 29;
    }
    return 0;
}
