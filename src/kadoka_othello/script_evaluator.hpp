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
    NativeProcess,
    Wasm,
};

struct ScriptEvaluatorConfig {
    ScriptEvaluatorRuntime runtime{ScriptEvaluatorRuntime::PythonProcess};
    std::string entry_path;
    std::string executable{"python"};
};

class ScriptEvaluator {
public:
    explicit ScriptEvaluator(ScriptEvaluatorConfig config);

    [[nodiscard]] const ScriptEvaluatorConfig& config() const noexcept;

    [[nodiscard]] std::vector<ScriptEvaluatorResult> evaluate_batch(
        const std::vector<ScriptEvaluatorCase>& cases) const;

private:
    [[nodiscard]] std::string build_command(
        const std::string& input_path,
        const std::string& output_path) const;

    ScriptEvaluatorConfig config_;
};

[[nodiscard]] ScriptEvaluatorConfig load_script_evaluator_config(
    const std::string& path,
    const std::string& default_entry_path = {});

[[nodiscard]] ScriptEvaluator load_script_evaluator_asset(
    const ModelRootDescriptor& model,
    const std::string& asset_id);

[[nodiscard]] const char* script_evaluator_runtime_name(
    ScriptEvaluatorRuntime runtime) noexcept;

}  // namespace kadoka::othello
