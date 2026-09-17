#include "kadoka_othello/evaluator_ai.hpp"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace kadoka::othello {
namespace {

const ModelAssetDescriptor& evaluator_asset(const ModelRootDescriptor& model) {
    const ModelAssetDescriptor* selected = nullptr;
    for (const auto& asset : model.assets) {
        if (asset.type != "kadoka.script_evaluator.v1") continue;
        if (asset.id == "evaluator") return asset;
        if (selected != nullptr) {
            throw std::runtime_error(
                "evaluator AI model has multiple script evaluators; name the primary asset 'evaluator'");
        }
        selected = &asset;
    }
    if (selected == nullptr) {
        throw std::runtime_error("evaluator AI model requires a kadoka.script_evaluator.v1 asset");
    }
    return *selected;
}

std::vector<ScriptEvaluatorCase> make_cases(const AdaptedAIInput& input) {
    if (input.board == nullptr) {
        throw std::invalid_argument("EvaluatorAI requires board input");
    }
    if (input.legal_moves == nullptr || input.legal_moves->empty()) {
        throw std::invalid_argument("EvaluatorAI requires non-empty legal moves");
    }

    const double board_size = static_cast<double>(input.board->size());
    const double denominator = board_size > 1.0 ? board_size - 1.0 : 1.0;
    const double center = (board_size - 1.0) * 0.5;

    std::vector<ScriptEvaluatorCase> cases;
    cases.reserve(input.legal_moves->size());
    for (std::size_t index = 0; index < input.legal_moves->size(); ++index) {
        const Position move = input.legal_moves->at(index);
        const bool top_or_bottom = move.row == 0 || move.row + 1 == input.board->size();
        const bool left_or_right = move.col == 0 || move.col + 1 == input.board->size();
        const bool corner = top_or_bottom && left_or_right;
        const bool edge = top_or_bottom || left_or_right;
        const double distance =
            (std::abs(static_cast<double>(move.row) - center) +
             std::abs(static_cast<double>(move.col) - center)) /
            denominator;

        ScriptEvaluatorCase item;
        item.id = index;
        item.features = {
            {"row", static_cast<double>(move.row)},
            {"col", static_cast<double>(move.col)},
            {"row_normalized", static_cast<double>(move.row) / denominator},
            {"col_normalized", static_cast<double>(move.col) / denominator},
            {"is_corner", corner ? 1.0 : 0.0},
            {"is_edge", edge ? 1.0 : 0.0},
            {"center_distance", distance},
            {"legal_move_count", static_cast<double>(input.legal_moves->size())},
        };
        cases.push_back(std::move(item));
    }
    return cases;
}

}  // namespace

EvaluatorAI::EvaluatorAI(
    std::string package_id,
    const ModelRootDescriptor& model)
    : package_id_(std::move(package_id)),
      evaluator_(load_script_evaluator_asset(model, evaluator_asset(model).id)) {
    if (model.engine != "kadoka.evaluator_ai.v1") {
        throw std::invalid_argument(
            "EvaluatorAI requires model engine kadoka.evaluator_ai.v1");
    }
}

std::string EvaluatorAI::id() const {
    return package_id_;
}

AIOutput EvaluatorAI::think(const AdaptedAIInput& input) {
    return inspect(input).output;
}

AIInspection EvaluatorAI::inspect(const AdaptedAIInput& input) {
    const auto cases = make_cases(input);
    const auto results = evaluator_.evaluate_batch(cases);

    std::unordered_map<std::size_t, const ScriptEvaluatorResult*> by_id;
    by_id.reserve(results.size());
    for (const auto& result : results) {
        if (result.id >= cases.size()) {
            throw std::runtime_error("evaluator AI returned out-of-range candidate id");
        }
        if (!by_id.emplace(result.id, &result).second) {
            throw std::runtime_error("evaluator AI returned duplicate candidate id");
        }
    }
    if (by_id.size() != cases.size()) {
        throw std::runtime_error("evaluator AI did not return every candidate result");
    }

    AIInspection inspection;
    inspection.candidates.reserve(cases.size());

    bool selected = false;
    std::size_t selected_index = 0;
    double selected_value = 0.0;
    for (std::size_t index = 0; index < cases.size(); ++index) {
        const auto found = by_id.find(index);
        if (found == by_id.end()) {
            throw std::runtime_error("evaluator AI result mapping is incomplete");
        }
        const ScriptEvaluatorResult& result = *found->second;
        const double value = result.value_or("score", 0.0);
        const double policy = result.value_or(
            "policy",
            result.value_or("confidence", 0.0));
        inspection.candidates.push_back({input.legal_moves->at(index), value, policy});

        if (!selected || value > selected_value) {
            selected = true;
            selected_index = index;
            selected_value = value;
        }
    }

    if (!selected) {
        throw std::runtime_error("evaluator AI failed to select a move");
    }

    inspection.output = AIOutput{input.legal_moves->at(selected_index)};
    inspection.diagnostics.push_back({"engine", "kadoka.evaluator_ai.v1"});
    inspection.diagnostics.push_back({
        "evaluator_runtime",
        script_evaluator_runtime_name(evaluator_.config().runtime)});
    inspection.diagnostics.push_back({
        "candidate_count",
        std::to_string(cases.size())});

    const ScriptEvaluatorResult& chosen = *by_id.at(selected_index);
    for (const auto& diagnostic : chosen.diagnostics) {
        inspection.diagnostics.push_back({diagnostic.first, diagnostic.second});
    }
    return inspection;
}

}  // namespace kadoka::othello
