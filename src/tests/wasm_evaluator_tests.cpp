#include "kadoka_othello/script_evaluator.hpp"
#include "test_support.hpp"

#include <cmath>
#include <filesystem>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

using namespace kadoka::othello;

namespace {

void require_values(const std::vector<ScriptEvaluatorResult>& results, const std::vector<ScriptEvaluatorCase>& cases,
                    const std::vector<double>& scores, const std::string& runtime) {
    KADOKA_REQUIRE(results.size() == cases.size());
    for (std::size_t i = 0; i < cases.size(); ++i) {
        KADOKA_REQUIRE(results[i].id == cases[i].id);
        const double score = results[i].value_or("score", std::numeric_limits<double>::quiet_NaN());
        KADOKA_REQUIRE(std::isfinite(score));
        KADOKA_REQUIRE(std::abs(score - scores[i]) < 1e-8);
        KADOKA_REQUIRE(results[i].value_or("confidence", -1.0) == 1.0);
        bool found_runtime = false;
        for (const auto& diagnostic : results[i].diagnostics) {
            if (diagnostic.first == "runtime" && diagnostic.second == runtime)
                found_runtime = true;
        }
        KADOKA_REQUIRE(found_runtime);
    }
}

void require_failure(ScriptEvaluator& evaluator, int mode, const std::string& expected) {
    bool rejected = false;
    try {
        (void)evaluator.evaluate_batch({{7, {{"failure_mode", static_cast<double>(mode)}}}});
    } catch (const std::runtime_error& error) {
        KADOKA_REQUIRE(std::string(error.what()).find(expected) != std::string::npos);
        rejected = true;
    }
    KADOKA_REQUIRE(rejected);
}

} // namespace

int main(int argc, char** argv) {
    try {
        KADOKA_REQUIRE(argc == 5);
        ScriptEvaluator wasm(load_script_evaluator_config(argv[1]));
        ScriptEvaluator failure(load_script_evaluator_config(argv[2]));
        ScriptEvaluator python(load_script_evaluator_config(argv[3]));
        ScriptEvaluator native(load_script_evaluator_config(argv[4]));
        KADOKA_REQUIRE(wasm.config().runtime == ScriptEvaluatorRuntime::Wasm);
        KADOKA_REQUIRE(std::filesystem::path(wasm.config().wasm_cache_config).is_absolute());
        KADOKA_REQUIRE(std::filesystem::is_regular_file(wasm.config().wasm_cache_config));
        KADOKA_REQUIRE(wasm.evaluate_batch({}).empty());

        // All reference formulas agree for saturated confidence and zero delay.
        const std::vector<ScriptEvaluatorCase> shared{
            {42, {{"memory_bonus", 20.0}, {"recall_delay", 0.0}}},
            {7, {{"memory_bonus", 10.0}, {"recall_delay", 0.0}}},
            {99, {{"memory_bonus", 30.0}, {"recall_delay", 0.0}}},
        };
        const std::vector<double> shared_scores{20.0, 10.0, 30.0};
        require_values(wasm.evaluate_batch(shared), shared, shared_scores, "wasm");
        require_values(native.evaluate_batch(shared), shared, shared_scores, "native_process");
        require_values(python.evaluate_batch(shared), shared, shared_scores, "python_process");

        const std::vector<ScriptEvaluatorCase> varied{
            {71, {{"memory_bonus", 2.5}, {"recall_delay", 0.5}}},
            {0, {{"memory_bonus", -4.0}, {"recall_delay", 1.0}}},
            {8, {}},
        };
        const std::vector<double> varied_scores{2.0, -5.0, 0.0};
        require_values(wasm.evaluate_batch(varied), varied, varied_scores, "wasm");
        require_values(native.evaluate_batch(varied), varied, varied_scores, "native_process");

        require_failure(failure, 1, "process failed runtime=wasm");
        require_failure(failure, 2, "did not create output");
        require_failure(failure, 3, "invalid output");
        require_failure(failure, 4, "mismatched result ids");
        require_failure(failure, 5, "malformed numeric value");
        require_failure(failure, 6, "unterminated result");
        require_failure(failure, 7, "unexpected result count");

        ScriptEvaluatorConfig missing = wasm.config();
        missing.entry_path += ".missing";
        ScriptEvaluator absent(std::move(missing));
        require_failure(absent, 0, "process failed runtime=wasm");
        std::cout << "WASM batch parity and failure contract passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
