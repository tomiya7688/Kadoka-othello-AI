#pragma once

#include <memory>
#include <string>
#include <vector>

#include "kadoka_othello/script_evaluator.hpp"

namespace kadoka::othello {

class NativeInProcessEvaluator {
public:
    NativeInProcessEvaluator(std::string library_path, std::string symbol_name);

    [[nodiscard]] std::vector<ScriptEvaluatorResult> evaluate_batch(
        const std::vector<ScriptEvaluatorCase>& cases) const;

    [[nodiscard]] const std::string& library_path() const noexcept;
    [[nodiscard]] const std::string& symbol_name() const noexcept;

private:
    struct Impl;
    std::shared_ptr<Impl> impl_;
};

}  // namespace kadoka::othello
