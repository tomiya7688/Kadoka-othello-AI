#pragma once

#include <string>

#include "kadoka_othello/ai.hpp"
#include "kadoka_othello/model_descriptor.hpp"
#include "kadoka_othello/script_evaluator.hpp"

namespace kadoka::othello {

class EvaluatorAI final : public IAIEngine {
public:
    EvaluatorAI(std::string package_id, const ModelRootDescriptor& model);

    [[nodiscard]] std::string id() const override;
    [[nodiscard]] AIOutput think(const AdaptedAIInput& input) override;
    [[nodiscard]] AIInspection inspect(const AdaptedAIInput& input) override;

private:
    std::string package_id_;
    ScriptEvaluator evaluator_;
};

}  // namespace kadoka::othello
