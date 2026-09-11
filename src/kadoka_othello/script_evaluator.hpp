#pragma once

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "kadoka_othello/model_descriptor.hpp"

namespace kadoka::othello {

struct ScriptEvaluatorFeature {
    std::string key;
    double value{};
};

struct ScriptEvaluatorCase {
    std::size_t id{};
    std::vector<ScriptEvaluatorFeature> features;
};

struct ScriptEvaluatorValue {
    std::string key;
    double value{};
};

struct ScriptEvaluatorResult {
    std::size_t id{};
    std::vector<ScriptEvaluatorValue> values;
    std::vector<std::pair<std::string, std::string>> diagnostics;

    [[nodiscard]] double value_or(
        const std::string& key,
        double fallback = 0.0) const noexcept;
};

enum class ScriptEvaluatorRuntime {
    PythonProcess,
};

struct ScriptEvaluatorConfig {
    ScriptEvaluatorRuntime runtime{ScriptEvaluatorRuntime::PythonProcess};
    std::string script_path;
    std::string executable{"python"};
};

class ScriptEvaluator {
public:
    explicit ScriptEvaluator(ScriptEvaluatorConfig config);

    [[nodiscard]] const ScriptEvaluatorConfig& config() const noexcept;

    [[nodiscard]] std::vector<ScriptEvaluatorResult> evaluate_batch(
        const std::vector<ScriptEvaluatorCase>& cases) const;

private:
    ScriptEvaluatorConfig config_;
};

[[nodiscard]] ScriptEvaluatorConfig load_script_evaluator_config(
    const std::string& path,
    const std::string& default_script_path = {});

[[nodiscard]] ScriptEvaluator load_script_evaluator_asset(
    const ModelRootDescriptor& model,
    const std::string& asset_id);

}  // namespace kadoka::othello
