#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#define KADOKA_SCRIPT_EVALUATOR_ABI_VERSION 1
#define KADOKA_SCRIPT_EVALUATOR_V1_SYMBOL "kadoka_script_evaluator_v1"

typedef struct KadokaScriptEvaluatorFeatureV1 {
    const char* key;
    double value;
} KadokaScriptEvaluatorFeatureV1;

typedef struct KadokaScriptEvaluatorCaseV1 {
    size_t id;
    const KadokaScriptEvaluatorFeatureV1* features;
    size_t feature_count;
} KadokaScriptEvaluatorCaseV1;

typedef struct KadokaScriptEvaluatorSinkV1 {
    void* context;
    int (*begin_result)(void* context, size_t id);
    int (*value)(void* context, const char* key, double value);
    int (*diagnostic)(void* context, const char* key, const char* value);
    int (*end_result)(void* context);
} KadokaScriptEvaluatorSinkV1;

typedef int (*KadokaScriptEvaluatorFunctionV1)(
    const KadokaScriptEvaluatorCaseV1* cases,
    size_t case_count,
    const KadokaScriptEvaluatorSinkV1* sink);

/*
 * Native in-process evaluator modules export a function named
 * KADOKA_SCRIPT_EVALUATOR_V1_SYMBOL with KadokaScriptEvaluatorFunctionV1.
 *
 * Return 0 on success. Non-zero values indicate evaluator failure.
 * Strings passed to sink callbacks only need to remain valid for the duration
 * of the callback because the host copies them immediately.
 */

#ifdef __cplusplus
}
#endif
