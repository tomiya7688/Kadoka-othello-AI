#include "kadoka_othello/evaluator_ai.hpp"

#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <utility>

#include "kadoka_othello/rules.hpp"

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

std::vector<Position> derive_legal_moves(const AIInput& input) {
    if (input.board == nullptr) {
        throw std::invalid_argument("EvaluatorAI requires board input");
    }
    std::vector<Position> legal_moves =
        rules::legal_moves(*input.board, input.side_to_move);
    if (legal_moves.empty()) {
        throw std::invalid_argument("EvaluatorAI requires at least one legal move");
    }
    return legal_moves;
}

std::vector<ScriptEvaluatorCase> make_cases(
    const AIInput& input,
    const std::vector<Position>& legal_moves) {
    const double board_size = static_cast<double>(input.board->size());
    const double denominator = board_size > 1.0 ? board_size - 1.0 : 1.0;
    const double center = (board_size - 1.0) * 0.5;

    std::vector<ScriptEvaluatorCase> cases;
    cases.reserve(legal_moves.size());
    for (std::size_t index = 0; index < legal_moves.size(); ++index) {
        const Position move = legal_moves[index];
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
            {"legal_move_count", static_cast<double>(legal_moves.size())},
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

AIOutput EvaluatorAI::think(const AIInput& input) {
    return inspect(input).output;
}

AIInspection EvaluatorAI::inspect(const AIInput& input) {
    const std::vector<Position> legal_moves = derive_legal_moves(input);
    const auto cases = make_cases(input, legal_moves);
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
        const ScriptEvaluatorResult& result = *by_id.at(index);
        const double value = result.value_or("score", 0.0);
        const double policy = result.value_or(
            "policy",
            result.value_or("confidence", 0.0));
        inspection.candidates.push_back({legal_moves[index], value, policy});

        if (!selected || value > selected_value) {
            selected = true;
            selected_index = index;
            selected_value = value;
        }
    }

    if (!selected) {
        throw std::runtime_error("evaluator AI failed to select a move");
    }

    inspection.output = AIOutput{legal_moves[selected_index]};
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
