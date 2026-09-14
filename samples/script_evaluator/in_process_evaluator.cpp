#include <algorithm>
#include <cmath>
#include <cstring>

#include "kadoka_othello/script_evaluator_abi.h"

#ifdef _WIN32
#define KADOKA_EVALUATOR_EXPORT __declspec(dllexport)
#else
#define KADOKA_EVALUATOR_EXPORT __attribute__((visibility("default")))
#endif

namespace {

bool ends_with(const char* value, const char* suffix) {
    if (value == nullptr || suffix == nullptr) return false;
    const std::size_t value_length = std::strlen(value);
    const std::size_t suffix_length = std::strlen(suffix);
    if (suffix_length > value_length) return false;
    return std::strcmp(value + value_length - suffix_length, suffix) == 0;
}

double evaluate_score(const KadokaScriptEvaluatorCaseV1& item) {
    double score = 0.0;
    for (std::size_t i = 0; i < item.feature_count; ++i) {
        const auto& feature = item.features[i];
        if (ends_with(feature.key, "bonus")) score += feature.value;
        else if (ends_with(feature.key, "penalty")) score -= feature.value;
        else score += feature.value * 0.1;
    }
    return score;
}

}  // namespace

extern "C" KADOKA_EVALUATOR_EXPORT int kadoka_script_evaluator_v1(
    const KadokaScriptEvaluatorCaseV1* cases,
    std::size_t case_count,
    const KadokaScriptEvaluatorSinkV1* sink) {
    if (cases == nullptr || sink == nullptr || sink->context == nullptr) return 10;
    if (sink->begin_result == nullptr || sink->value == nullptr ||
        sink->diagnostic == nullptr || sink->end_result == nullptr) {
        return 11;
    }

    for (std::size_t i = 0; i < case_count; ++i) {
        const double score = evaluate_score(cases[i]);
        const double confidence = std::min(1.0, std::abs(score) / 10.0);

        if (sink->begin_result(sink->context, cases[i].id) != 0) return 20;
        if (sink->value(sink->context, "score", score) != 0) return 21;
        if (sink->value(sink->context, "confidence", confidence) != 0) return 22;
        if (sink->diagnostic(sink->context, "runtime", "native_in_process") != 0) return 23;
        if (sink->end_result(sink->context) != 0) return 24;
    }
    return 0;
}
