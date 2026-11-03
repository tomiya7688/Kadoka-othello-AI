#include "kadoka_othello/evaluator_ai.hpp"

#include "kadoka_othello/rules.hpp"

#include <array>
#include <cmath>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace kadoka::othello {
namespace {

const ModelAssetDescriptor& evaluator_asset(const ModelRootDescriptor& model) {
    if (model.engine != "kadoka.evaluator_ai.v1") {
        throw std::invalid_argument("EvaluatorAI requires model engine kadoka.evaluator_ai.v1");
    }
    const ModelAssetDescriptor* selected = nullptr;
    for (const auto& asset : model.assets) {
        if (!asset.required)
            continue;
        if (asset.type != "kadoka.script_evaluator.v1")
            throw std::runtime_error("unsupported required evaluator AI model asset: " + asset.id + " (" + asset.type +
                                     ")");
        if (selected != nullptr) {
            throw std::runtime_error("evaluator AI model has multiple required script evaluator assets");
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
    std::vector<Position> legal_moves = rules::legal_moves(*input.board, input.side_to_move);
    if (legal_moves.empty()) {
        throw std::invalid_argument("EvaluatorAI requires at least one legal move");
    }
    return legal_moves;
}

std::vector<ScriptEvaluatorCase> make_cases(const AIInput& input, const std::vector<Position>& legal_moves) {
    const double board_size = static_cast<double>(input.board->size());
    const double denominator = board_size > 1.0 ? board_size - 1.0 : 1.0;
    const double center = (board_size - 1.0) * 0.5;
    const double board_area = board_size * board_size;
    const std::array<Position, 4> corners{{
        {0, 0},
        {0, input.board->size() - 1},
        {input.board->size() - 1, 0},
        {input.board->size() - 1, input.board->size() - 1},
    }};

    std::vector<ScriptEvaluatorCase> cases;
    cases.reserve(legal_moves.size());
    for (std::size_t index = 0; index < legal_moves.size(); ++index) {
        const Position move = legal_moves[index];
        const bool top_or_bottom = move.row == 0 || move.row + 1 == input.board->size();
        const bool left_or_right = move.col == 0 || move.col + 1 == input.board->size();
        const bool corner = top_or_bottom && left_or_right;
        const bool edge = top_or_bottom || left_or_right;
        const double distance =
            (std::abs(static_cast<double>(move.row) - center) + std::abs(static_cast<double>(move.col) - center)) /
            denominator;
        const std::vector<Position> flips = rules::flips_for_move(*input.board, move, input.side_to_move);
        if (flips.empty()) {
            throw std::logic_error("EvaluatorAI legal move produced no flips");
        }
        Board resulting_board = *input.board;
        resulting_board.set(move, to_cell(input.side_to_move));
        for (const Position flipped : flips) {
            resulting_board.set(flipped, to_cell(input.side_to_move));
        }
        const std::size_t opponent_mobility = rules::legal_moves(resulting_board, opponent(input.side_to_move)).size();
        const double disc_difference_after_move =
            static_cast<double>(resulting_board.count(to_cell(input.side_to_move))) -
            static_cast<double>(resulting_board.count(to_cell(opponent(input.side_to_move))));
        bool adjacent_to_unoccupied_corner = false;
        for (const Position corner : corners) {
            const int row_distance = std::abs(static_cast<int>(move.row) - static_cast<int>(corner.row));
            const int col_distance = std::abs(static_cast<int>(move.col) - static_cast<int>(corner.col));
            if (resulting_board.at(corner) == Cell::Empty && row_distance <= 1 && col_distance <= 1 && move != corner) {
                adjacent_to_unoccupied_corner = true;
                break;
            }
        }

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
            {"flipped_disc_count", static_cast<double>(flips.size())},
            {"opponent_legal_move_count", static_cast<double>(opponent_mobility)},
            {"opponent_mobility", static_cast<double>(opponent_mobility) / board_area},
            {"disc_difference_after_move", disc_difference_after_move},
            {"adjacent_to_unoccupied_corner", adjacent_to_unoccupied_corner ? 1.0 : 0.0},
        };
        cases.push_back(std::move(item));
    }
    return cases;
}

} // namespace

EvaluatorAI::EvaluatorAI(std::string package_id, const ModelRootDescriptor& model)
    : package_id_(std::move(package_id)), evaluator_(load_script_evaluator_asset(model, evaluator_asset(model).id)) {}

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
        const double policy = result.value_or("policy", result.value_or("confidence", 0.0));
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
    inspection.diagnostics.push_back({"evaluator_runtime", script_evaluator_runtime_name(evaluator_.config().runtime)});
    inspection.diagnostics.push_back({"candidate_count", std::to_string(cases.size())});

    const ScriptEvaluatorResult& chosen = *by_id.at(selected_index);
    for (const auto& diagnostic : chosen.diagnostics) {
        inspection.diagnostics.push_back({diagnostic.first, diagnostic.second});
    }
    return inspection;
}

} // namespace kadoka::othello
