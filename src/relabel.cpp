#include "kadoka_othello/relabel.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <optional>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <unordered_set>
#include <utility>

#include "kadoka_othello/package_loader.hpp"
#include "kadoka_othello/rules.hpp"

namespace kadoka::othello {
namespace {

constexpr std::array<std::pair<int, int>, 8> kDirections{{
    {-1, -1}, {-1, 0}, {-1, 1},
    {0, -1},            {0, 1},
    {1, -1},   {1, 0},  {1, 1},
}};

bool is_legal_position(
    Position move,
    const std::vector<Position>& legal_moves) {
    return std::find(
               legal_moves.begin(),
               legal_moves.end(),
               move) != legal_moves.end();
}

std::string escape_json(std::string_view value) {
    std::string escaped;
    escaped.reserve(value.size() + 8);
    for (const char ch : value) {
        switch (ch) {
            case '\\': escaped += "\\\\"; break;
            case '"': escaped += "\\\""; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped += ch; break;
        }
    }
    return escaped;
}

const char* player_name(Player player) noexcept {
    return player == Player::Black ? "black" : "white";
}

void write_position_or_null(
    std::ostringstream& out,
    const std::optional<Position>& move) {
    if (!move) {
        out << "null";
        return;
    }
    out << "{\"row\":" << move->row
        << ",\"col\":" << move->col << '}';
}

void write_optional_double(
    std::ostringstream& out,
    const std::optional<double>& value) {
    if (value) out << std::setprecision(17) << *value;
    else out << "null";
}

void write_optional_u64(
    std::ostringstream& out,
    const std::optional<std::uint64_t>& value) {
    if (value) out << *value;
    else out << "null";
}

void write_optional_size(
    std::ostringstream& out,
    const std::optional<std::size_t>& value) {
    if (value) out << *value;
    else out << "null";
}

void write_metrics(
    std::ostringstream& out,
    const AIMoveMetrics& metrics) {
    out << "{\"nodes\":";
    write_optional_u64(out, metrics.nodes);
    out << ",\"simulations\":";
    write_optional_u64(out, metrics.simulations);
    out << ",\"depth\":";
    write_optional_size(out, metrics.depth);
    out << ",\"search_effort\":";
    write_optional_double(out, metrics.search_effort);
    out << '}';
}

std::uint64_t mix64(std::uint64_t value) noexcept {
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30U)) *
        0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) *
        0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

std::uint64_t hash_text(
    std::uint64_t seed,
    std::string_view text) noexcept {
    std::uint64_t value = seed;
    for (const unsigned char ch : text) {
        value ^= static_cast<std::uint64_t>(ch);
        value *= 1099511628211ULL;
    }
    return value;
}

std::uint64_t relabel_engine_seed(
    const RelabelEngineSpec& spec,
    const RelabelPosition& position,
    std::size_t engine_index) noexcept {
    std::uint64_t value = mix64(
        spec.seed ^
        (static_cast<std::uint64_t>(position.ply) << 32U) ^
        static_cast<std::uint64_t>(engine_index + 1));
    value = hash_text(value, position.game_id);
    return value == 0 ? 1 : value;
}

bool in_bounds_signed(
    const Board& board,
    int row,
    int col) noexcept {
    return row >= 0 && col >= 0 &&
        row < static_cast<int>(board.size()) &&
        col < static_cast<int>(board.size());
}

std::size_t frontier_count(
    const Board& board,
    Player player) {
    std::size_t count = 0;
    const Cell own = to_cell(player);
    for (std::size_t row = 0; row < board.size(); ++row) {
        for (std::size_t col = 0; col < board.size(); ++col) {
            const Position position{row, col};
            if (board.at(position) != own) continue;

            bool frontier = false;
            for (const auto [dr, dc] : kDirections) {
                const int next_row = static_cast<int>(row) + dr;
                const int next_col = static_cast<int>(col) + dc;
                if (!in_bounds_signed(
                        board,
                        next_row,
                        next_col)) {
                    continue;
                }
                if (board.at({
                        static_cast<std::size_t>(next_row),
                        static_cast<std::size_t>(next_col)}) ==
                    Cell::Empty) {
                    frontier = true;
                    break;
                }
            }
            if (frontier) ++count;
        }
    }
    return count;
}

std::size_t potential_mobility(
    const Board& board,
    Player player) {
    const Cell enemy = to_cell(opponent(player));
    std::size_t count = 0;
    for (std::size_t row = 0; row < board.size(); ++row) {
        for (std::size_t col = 0; col < board.size(); ++col) {
            const Position position{row, col};
            if (board.at(position) != Cell::Empty) continue;

            bool touches_enemy = false;
            for (const auto [dr, dc] : kDirections) {
                const int next_row = static_cast<int>(row) + dr;
                const int next_col = static_cast<int>(col) + dc;
                if (!in_bounds_signed(
                        board,
                        next_row,
                        next_col)) {
                    continue;
                }
                if (board.at({
                        static_cast<std::size_t>(next_row),
                        static_cast<std::size_t>(next_col)}) ==
                    enemy) {
                    touches_enemy = true;
                    break;
                }
            }
            if (touches_enemy) ++count;
        }
    }
    return count;
}

std::size_t stable_corner_edge_estimate(
    const Board& board,
    Player player) {
    if (board.size() == 0) return 0;

    const Cell own = to_cell(player);
    const std::size_t last = board.size() - 1;
    const std::array<Position, 4> corners{{
        {0, 0},
        {0, last},
        {last, 0},
        {last, last},
    }};

    std::vector<bool> counted(
        board.size() * board.size(),
        false);
    auto mark = [&](Position position) {
        counted[position.row * board.size() + position.col] =
            true;
    };

    for (const Position corner : corners) {
        if (board.at(corner) != own) continue;
        mark(corner);

        const int row_dir =
            corner.row == 0 ? 1 : -1;
        const int col_dir =
            corner.col == 0 ? 1 : -1;

        for (int step = 1;
             step < static_cast<int>(board.size());
             ++step) {
            const int row =
                static_cast<int>(corner.row) +
                row_dir * step;
            if (!in_bounds_signed(
                    board,
                    row,
                    static_cast<int>(corner.col))) {
                break;
            }
            const Position position{
                static_cast<std::size_t>(row),
                corner.col,
            };
            if (board.at(position) != own) break;
            mark(position);
        }

        for (int step = 1;
             step < static_cast<int>(board.size());
             ++step) {
            const int col =
                static_cast<int>(corner.col) +
                col_dir * step;
            if (!in_bounds_signed(
                    board,
                    static_cast<int>(corner.row),
                    col)) {
                break;
            }
            const Position position{
                corner.row,
                static_cast<std::size_t>(col),
            };
            if (board.at(position) != own) break;
            mark(position);
        }
    }

    return static_cast<std::size_t>(
        std::count(counted.begin(), counted.end(), true));
}

std::size_t legal_corner_count(
    const Board& board,
    Player player) {
    const std::size_t last = board.size() - 1;
    const std::array<Position, 4> corners{{
        {0, 0},
        {0, last},
        {last, 0},
        {last, last},
    }};

    std::size_t count = 0;
    for (const Position corner : corners) {
        if (rules::is_legal_move(
                board,
                corner,
                player)) {
            ++count;
        }
    }
    return count;
}

std::optional<RelabelCandidateEvaluation>
find_engine_candidate(
    const AIInspection& inspection,
    Position move) {
    for (const auto& candidate : inspection.candidates) {
        if (candidate.move == move) {
            RelabelCandidateEvaluation result;
            result.move = move;
            result.value = candidate.value;
            result.policy = candidate.policy;
            result.q = candidate.q;
            return result;
        }
    }
    return std::nullopt;
}

std::vector<RelabelCandidateEvaluation>
filter_engine_candidates(
    const AIInspection& inspection,
    const std::vector<Position>& legal_moves,
    RelabelCandidateMode mode,
    std::size_t top_k) {
    std::vector<RelabelCandidateEvaluation> result;

    if (mode == RelabelCandidateMode::AllLegal) {
        result.reserve(legal_moves.size());
        for (const Position move : legal_moves) {
            const auto candidate =
                find_engine_candidate(inspection, move);
            if (candidate) {
                result.push_back(*candidate);
            } else {
                result.push_back(
                    RelabelCandidateEvaluation{move});
            }
        }
        return result;
    }

    if (top_k == 0) {
        throw std::invalid_argument(
            "TopK relabel mode requires top_k > 0");
    }

    for (const auto& candidate : inspection.candidates) {
        if (!is_legal_position(
                candidate.move,
                legal_moves)) {
            continue;
        }
        result.push_back(RelabelCandidateEvaluation{
            candidate.move,
            candidate.value,
            candidate.policy,
            candidate.q,
            false,
        });
    }

    std::stable_sort(
        result.begin(),
        result.end(),
        [](const RelabelCandidateEvaluation& lhs,
           const RelabelCandidateEvaluation& rhs) {
            const double lhs_value =
                lhs.value.value_or(
                    -std::numeric_limits<double>::infinity());
            const double rhs_value =
                rhs.value.value_or(
                    -std::numeric_limits<double>::infinity());
            return lhs_value > rhs_value;
        });

    if (result.size() > top_k) {
        result.resize(top_k);
    }

    if (is_legal_position(
            inspection.output.move,
            legal_moves)) {
        const bool selected_present =
            std::any_of(
                result.begin(),
                result.end(),
                [&](const RelabelCandidateEvaluation& item) {
                    return item.move ==
                        inspection.output.move;
                });
        if (!selected_present) {
            RelabelCandidateEvaluation selected{
                inspection.output.move,
            };
            if (result.size() < top_k) {
                result.push_back(std::move(selected));
            } else if (!result.empty()) {
                result.back() = std::move(selected);
            }
        }
    }
    return result;
}

struct ExactSearchContext {
    std::uint64_t max_nodes{};
    std::uint64_t nodes{};
    std::chrono::steady_clock::time_point deadline;
    bool exhausted{false};
};

bool exact_budget_available(
    ExactSearchContext& context) {
    if (context.nodes >= context.max_nodes) {
        context.exhausted = true;
        return false;
    }
    if (std::chrono::steady_clock::now() >=
        context.deadline) {
        context.exhausted = true;
        return false;
    }
    return true;
}

int terminal_disc_difference(
    const Board& board,
    Player perspective) {
    return static_cast<int>(
               board.count(to_cell(perspective))) -
        static_cast<int>(
            board.count(to_cell(opponent(perspective))));
}

std::optional<int> exact_search(
    const Board& board,
    Player side_to_move,
    Player perspective,
    bool previous_pass,
    int alpha,
    int beta,
    ExactSearchContext& context) {
    if (!exact_budget_available(context)) {
        return std::nullopt;
    }
    ++context.nodes;

    const auto legal =
        rules::legal_moves(board, side_to_move);
    if (legal.empty()) {
        if (previous_pass ||
            !rules::has_legal_move(
                board,
                opponent(side_to_move))) {
            return terminal_disc_difference(
                board,
                perspective);
        }
        return exact_search(
            board,
            opponent(side_to_move),
            perspective,
            true,
            alpha,
            beta,
            context);
    }

    const bool maximizing =
        side_to_move == perspective;
    int best = maximizing
        ? std::numeric_limits<int>::min()
        : std::numeric_limits<int>::max();

    for (const Position move : legal) {
        Board child = board;
        if (!rules::apply_move(
                child,
                move,
                side_to_move)) {
            throw std::logic_error(
                "exact solver received illegal generated move");
        }

        const auto child_value = exact_search(
            child,
            opponent(side_to_move),
            perspective,
            false,
            alpha,
            beta,
            context);
        if (!child_value) return std::nullopt;

        if (maximizing) {
            best = std::max(best, *child_value);
            alpha = std::max(alpha, best);
        } else {
            best = std::min(best, *child_value);
            beta = std::min(beta, best);
        }
        if (beta <= alpha) break;
    }
    return best;
}

RelabelExactResult run_exact_relabel(
    const CoreState& state,
    const std::vector<Position>& legal_moves,
    std::uint64_t max_nodes,
    std::chrono::steady_clock::time_point deadline) {
    RelabelExactResult result;
    result.attempted = true;
    if (max_nodes == 0) {
        result.budget_exhausted = true;
        return result;
    }

    Board board = board_from_core_state(state);
    ExactSearchContext context{
        max_nodes,
        0,
        deadline,
        false,
    };

    if (legal_moves.empty()) {
        const auto value = exact_search(
            board,
            state.side_to_move,
            state.side_to_move,
            false,
            std::numeric_limits<int>::min() / 2,
            std::numeric_limits<int>::max() / 2,
            context);
        result.nodes = context.nodes;
        result.budget_exhausted = context.exhausted;
        if (value) {
            result.completed = true;
            result.final_disc_difference = *value;
        }
        return result;
    }

    bool complete = true;
    int best_value =
        std::numeric_limits<int>::min();
    std::optional<Position> best_move;

    for (const Position move : legal_moves) {
        if (!exact_budget_available(context)) {
            complete = false;
            break;
        }

        Board child = board;
        if (!rules::apply_move(
                child,
                move,
                state.side_to_move)) {
            throw std::logic_error(
                "exact solver candidate became illegal");
        }

        const auto value = exact_search(
            child,
            opponent(state.side_to_move),
            state.side_to_move,
            false,
            std::numeric_limits<int>::min() / 2,
            std::numeric_limits<int>::max() / 2,
            context);
        if (!value) {
            complete = false;
            break;
        }

        RelabelCandidateEvaluation candidate;
        candidate.move = move;
        candidate.value =
            static_cast<double>(*value);
        candidate.exact = true;
        result.candidates.push_back(
            std::move(candidate));

        if (!best_move || *value > best_value) {
            best_value = *value;
            best_move = move;
        }
    }

    result.nodes = context.nodes;
    result.budget_exhausted =
        context.exhausted || !complete;
    result.completed =
        complete &&
        result.candidates.size() ==
            legal_moves.size();
    if (result.completed && best_move) {
        result.selected_move = best_move;
        result.final_disc_difference = best_value;
    }
    return result;
}

const RelabelBoardParameters&
find_relabel_parameters(
    const std::vector<RelabelBoardParameters>& parameters,
    std::size_t board_size) {
    for (const auto& item : parameters) {
        if (item.board_size == board_size) return item;
    }
    throw std::invalid_argument(
        "missing relabel parameters for board size " +
        std::to_string(board_size));
}

RelabelDisagreement compute_disagreement(
    const std::vector<RelabelEngineResult>& engines) {
    RelabelDisagreement disagreement;

    std::vector<Position> moves;
    std::vector<double> selected_values;
    std::vector<Position> unique_moves;

    for (const auto& engine : engines) {
        if (!engine.selected_move_legal) continue;
        moves.push_back(engine.selected_move);

        if (std::find(
                unique_moves.begin(),
                unique_moves.end(),
                engine.selected_move) ==
            unique_moves.end()) {
            unique_moves.push_back(
                engine.selected_move);
        }

        for (const auto& candidate : engine.candidates) {
            if (candidate.move ==
                    engine.selected_move &&
                candidate.value) {
                selected_values.push_back(
                    *candidate.value);
                break;
            }
        }
    }

    disagreement.move_disagreement =
        move_disagreement(moves);
    disagreement.selected_value_stddev =
        value_disagreement_stddev(
            selected_values);
    disagreement.distinct_selected_moves =
        unique_moves.size();
    return disagreement;
}

RelabelDerivedLabel derive_after_label(
    const RelabelExactResult& exact,
    const std::vector<RelabelEngineResult>& engines) {
    RelabelDerivedLabel label;

    if (exact.completed &&
        exact.selected_move &&
        exact.final_disc_difference) {
        label.selected_move =
            exact.selected_move;
        label.value =
            static_cast<double>(
                *exact.final_disc_difference);
        label.exact = true;
        label.source =
            "exact_endgame";
        return label;
    }

    const RelabelEngineResult* best = nullptr;
    for (const auto& engine : engines) {
        if (!engine.selected_move_legal) continue;
        if (best == nullptr ||
            engine.rating > best->rating ||
            (engine.rating == best->rating &&
             engine.rating_deviation <
                 best->rating_deviation)) {
            best = &engine;
        }
    }

    if (best != nullptr) {
        label.selected_move =
            best->selected_move;
        label.source =
            "engine:" + best->model_id +
            "@" + best->model_version;
        for (const auto& candidate : best->candidates) {
            if (candidate.move ==
                    best->selected_move &&
                candidate.value) {
                label.value = candidate.value;
                break;
            }
        }
    }
    return label;
}

std::string provenance_engine(
    const RelabelEngineResult& engine) {
    std::ostringstream out;
    out << "engine="
        << engine.model_id
        << '@' << engine.model_version
        << ";rating=" << std::setprecision(17)
        << engine.rating
        << ";rd=" << engine.rating_deviation;
    if (!engine.search_config.empty()) {
        out << ";search=" << engine.search_config;
    }
    return out.str();
}

void validate_relabel_input(
    const RelabelInput& input) {
    if (input.position.game_id.empty()) {
        throw std::invalid_argument(
            "relabel position requires game_id");
    }
    if (input.position.state.board_size !=
        input.confidence.board_size) {
        throw std::invalid_argument(
            "relabel confidence board_size mismatch");
    }
    if (input.position.game_id !=
        input.confidence.game_id ||
        input.position.ply !=
            input.confidence.ply) {
        throw std::invalid_argument(
            "relabel confidence sample must match game_id + ply");
    }
}

}  // namespace

RelabelPosition make_relabel_position(
    const BoardStateRecord& state,
    RelabelOriginalLabel original_label,
    std::string source_dataset_id) {
    RelabelPosition position;
    position.game_id = state.game_id;
    position.ply = state.ply;
    position.state = state.state;
    position.original_label =
        std::move(original_label);
    position.source_dataset_id =
        std::move(source_dataset_id);
    return position;
}

std::vector<RelabelBoardParameters>
default_relabel_board_parameters() {
    return {
        RelabelBoardParameters{6, 12},
        RelabelBoardParameters{8, 14},
        RelabelBoardParameters{10, 10},
    };
}

RelabelBoardAnalysis analyze_relabel_position(
    const CoreState& state) {
    const Board board =
        board_from_core_state(state);
    const Player self =
        state.side_to_move;
    const Player other =
        opponent(self);

    RelabelBoardAnalysis analysis;
    analysis.legal_move_count =
        rules::legal_moves(board, self).size();
    analysis.opponent_legal_move_count =
        rules::legal_moves(board, other).size();
    analysis.potential_mobility_self =
        potential_mobility(board, self);
    analysis.potential_mobility_opponent =
        potential_mobility(board, other);
    analysis.frontier_self =
        frontier_count(board, self);
    analysis.frontier_opponent =
        frontier_count(board, other);
    analysis.stable_corner_self =
        stable_corner_edge_estimate(
            board,
            self);
    analysis.stable_corner_opponent =
        stable_corner_edge_estimate(
            board,
            other);
    analysis.empty_count =
        board.count(Cell::Empty);
    analysis.mobility_difference =
        static_cast<int>(analysis.legal_move_count) -
        static_cast<int>(
            analysis.opponent_legal_move_count);
    analysis.potential_mobility_difference =
        static_cast<int>(
            analysis.potential_mobility_self) -
        static_cast<int>(
            analysis.potential_mobility_opponent);
    analysis.frontier_difference =
        static_cast<int>(analysis.frontier_self) -
        static_cast<int>(
            analysis.frontier_opponent);
    analysis.stable_corner_difference =
        static_cast<int>(
            analysis.stable_corner_self) -
        static_cast<int>(
            analysis.stable_corner_opponent);
    analysis.corner_availability_difference =
        static_cast<int>(
            legal_corner_count(board, self)) -
        static_cast<int>(
            legal_corner_count(board, other));
    analysis.parity =
        analysis.empty_count % 2 == 0 ? 1 : -1;
    return analysis;
}

RelabelBatchResult relabel_positions(
    const std::vector<RelabelInput>& inputs,
    const std::vector<RelabelEngineSpec>& engines,
    const IConfidenceCalculator& confidence_calculator,
    const std::vector<ConfidenceBoardParameters>&
        confidence_parameters,
    const std::vector<RelabelBoardParameters>&
        relabel_parameters,
    const RelabelBudget& budget,
    std::uint64_t sampler_seed) {
    if (budget.max_positions == 0) return {};
    if (engines.empty() &&
        !budget.enable_exact_endgame) {
        throw std::invalid_argument(
            "relabeling requires engines or exact solver");
    }
    if (budget.candidate_mode ==
            RelabelCandidateMode::TopK &&
        budget.top_k == 0) {
        throw std::invalid_argument(
            "TopK relabel mode requires top_k > 0");
    }

    for (const auto& input : inputs) {
        validate_relabel_input(input);
    }

    std::vector<ConfidenceSample> confidence_samples;
    confidence_samples.reserve(inputs.size());
    for (const auto& input : inputs) {
        confidence_samples.push_back(
            input.confidence);
    }

    const auto selections =
        select_reanalysis_samples(
            confidence_samples,
            confidence_calculator,
            confidence_parameters,
            std::min(
                budget.max_positions,
                inputs.size()),
            sampler_seed);

    std::vector<AIPackageManifest> manifests;
    manifests.reserve(engines.size());
    for (const auto& engine : engines) {
        manifests.push_back(
            load_ai_manifest(
                engine.manifest_path));
    }

    RelabelBatchResult batch;
    batch.selected_positions =
        selections.size();
    batch.records.reserve(
        selections.size());

    const auto start =
        std::chrono::steady_clock::now();
    const auto deadline =
        start +
        std::chrono::milliseconds(
            budget.max_elapsed_ms);

    for (const auto& selection : selections) {
        if (std::chrono::steady_clock::now() >= deadline) {
            batch.elapsed_budget_exhausted = true;
            break;
        }

        const RelabelInput& input =
            inputs[selection.sample_index];
        Board board =
            board_from_core_state(
                input.position.state);
        const auto legal_moves =
            rules::legal_moves(
                board,
                input.position.state.side_to_move);

        RelabelRecord record;
        record.game_id =
            input.position.game_id;
        record.ply =
            input.position.ply;
        record.board_size =
            input.position.state.board_size;
        record.side_to_move =
            input.position.state.side_to_move;
        record.source_dataset_id =
            input.position.source_dataset_id;
        record.before =
            input.position.original_label;
        record.confidence_before =
            selection.estimate;
        record.board_analysis =
            analyze_relabel_position(
                input.position.state);
        record.legal_moves =
            legal_moves;
        record.provenance.push_back(
            "confidence_calculator=" +
            confidence_calculator.id());
        if (selection.random_audit) {
            record.provenance.push_back(
                "selection=random_high_confidence_audit");
        } else {
            record.provenance.push_back(
                "selection=priority");
        }

        for (std::size_t engine_index = 0;
             engine_index < engines.size();
             ++engine_index) {
            if (batch.engine_calls >=
                budget.max_engine_calls) {
                batch.engine_budget_exhausted = true;
                break;
            }
            if (std::chrono::steady_clock::now() >=
                deadline) {
                batch.elapsed_budget_exhausted = true;
                break;
            }

            const auto& spec =
                engines[engine_index];
            const auto& manifest =
                manifests[engine_index];
            const std::uint64_t seed =
                relabel_engine_seed(
                    spec,
                    input.position,
                    engine_index);

            LoadedAIPackage package =
                load_ai_package(
                    manifest,
                    seed);
            const AIInput ai_input{
                &board,
                input.position.state.side_to_move,
                input.position.state.time,
            };
            const auto engine_start =
                std::chrono::steady_clock::now();
            AIInspection inspection =
                inspect_ai(
                    package.view(),
                    ai_input);
            const auto engine_end =
                std::chrono::steady_clock::now();
            ++batch.engine_calls;

            RelabelEngineResult engine_result;
            engine_result.model_id =
                manifest.id;
            engine_result.model_version =
                manifest.version;
            engine_result.rating =
                spec.rating;
            engine_result.rating_deviation =
                spec.rating_deviation;
            engine_result.search_config =
                spec.search_config;
            engine_result.seed =
                seed;
            engine_result.selected_move =
                inspection.output.move;
            engine_result.selected_move_legal =
                is_legal_position(
                    inspection.output.move,
                    legal_moves);
            engine_result.metrics =
                inspection.output.metrics;
            if (!engine_result.metrics.search_effort) {
                engine_result.metrics.search_effort =
                    std::chrono::duration<double, std::milli>(
                        engine_end -
                        engine_start).count();
            }
            engine_result.candidates =
                filter_engine_candidates(
                    inspection,
                    legal_moves,
                    budget.candidate_mode,
                    budget.top_k);
            engine_result.diagnostics =
                std::move(
                    inspection.diagnostics);

            record.provenance.push_back(
                provenance_engine(
                    engine_result));
            record.engines.push_back(
                std::move(
                    engine_result));
        }

        record.disagreement =
            compute_disagreement(
                record.engines);

        const auto& board_parameters =
            find_relabel_parameters(
                relabel_parameters,
                record.board_size);
        const bool exact_candidate =
            budget.enable_exact_endgame &&
            record.board_analysis.empty_count <=
                board_parameters
                    .exact_endgame_empty_threshold;

        if (exact_candidate &&
            !batch.elapsed_budget_exhausted) {
            const std::uint64_t remaining_nodes =
                batch.exact_nodes >=
                        budget.max_exact_nodes
                    ? 0
                    : budget.max_exact_nodes -
                        batch.exact_nodes;
            record.exact =
                run_exact_relabel(
                    input.position.state,
                    legal_moves,
                    remaining_nodes,
                    deadline);
            batch.exact_nodes +=
                record.exact.nodes;
            if (record.exact.budget_exhausted) {
                batch.exact_node_budget_exhausted =
                    batch.exact_nodes >=
                    budget.max_exact_nodes;
                if (std::chrono::steady_clock::now() >=
                    deadline) {
                    batch.elapsed_budget_exhausted =
                        true;
                }
            }
            if (record.exact.attempted) {
                record.provenance.push_back(
                    record.exact.completed
                        ? "exact_endgame=completed"
                        : "exact_endgame=incomplete");
            }
        }

        record.after =
            derive_after_label(
                record.exact,
                record.engines);
        batch.records.push_back(
            std::move(record));

        if (batch.elapsed_budget_exhausted) break;
    }

    return batch;
}

std::string relabel_record_to_json(
    const RelabelRecord& record) {
    std::ostringstream out;
    out << std::setprecision(17);
    out << "{\"format\":\""
        << kRelabelRecordFormat << "\""
        << ",\"game_id\":\""
        << escape_json(record.game_id) << "\""
        << ",\"ply\":" << record.ply
        << ",\"board_size\":" << record.board_size
        << ",\"side_to_move\":\""
        << player_name(record.side_to_move) << "\""
        << ",\"source_dataset_id\":\""
        << escape_json(record.source_dataset_id) << "\"";

    out << ",\"before\":{\"selected_move\":";
    write_position_or_null(
        out,
        record.before.selected_move);
    out << ",\"value\":";
    write_optional_double(
        out,
        record.before.value);
    out << ",\"source\":\""
        << escape_json(record.before.source) << "\"}";

    out << ",\"confidence_before\":{"
        << "\"confidence\":"
        << record.confidence_before.confidence
        << ",\"uncertainty\":"
        << record.confidence_before.uncertainty
        << ",\"priority\":"
        << record.confidence_before.reanalysis_priority
        << ",\"exact_endgame_candidate\":"
        << (record.confidence_before
                    .exact_endgame_candidate
                ? "true"
                : "false")
        << '}';

    out << ",\"board_analysis\":{"
        << "\"legal_move_count\":"
        << record.board_analysis.legal_move_count
        << ",\"opponent_legal_move_count\":"
        << record.board_analysis
               .opponent_legal_move_count
        << ",\"potential_mobility_self\":"
        << record.board_analysis
               .potential_mobility_self
        << ",\"potential_mobility_opponent\":"
        << record.board_analysis
               .potential_mobility_opponent
        << ",\"frontier_self\":"
        << record.board_analysis.frontier_self
        << ",\"frontier_opponent\":"
        << record.board_analysis.frontier_opponent
        << ",\"stable_corner_self\":"
        << record.board_analysis.stable_corner_self
        << ",\"stable_corner_opponent\":"
        << record.board_analysis
               .stable_corner_opponent
        << ",\"empty_count\":"
        << record.board_analysis.empty_count
        << ",\"mobility_difference\":"
        << record.board_analysis.mobility_difference
        << ",\"potential_mobility_difference\":"
        << record.board_analysis
               .potential_mobility_difference
        << ",\"frontier_difference\":"
        << record.board_analysis.frontier_difference
        << ",\"stable_corner_difference\":"
        << record.board_analysis
               .stable_corner_difference
        << ",\"corner_availability_difference\":"
        << record.board_analysis
               .corner_availability_difference
        << ",\"parity\":"
        << record.board_analysis.parity
        << '}';

    out << ",\"legal_moves\":[";
    for (std::size_t i = 0;
         i < record.legal_moves.size();
         ++i) {
        if (i) out << ',';
        out << "{\"row\":"
            << record.legal_moves[i].row
            << ",\"col\":"
            << record.legal_moves[i].col
            << '}';
    }
    out << ']';

    out << ",\"engines\":[";
    for (std::size_t i = 0;
         i < record.engines.size();
         ++i) {
        if (i) out << ',';
        const auto& engine =
            record.engines[i];
        out << "{\"model_id\":\""
            << escape_json(engine.model_id)
            << "\",\"model_version\":\""
            << escape_json(engine.model_version)
            << "\",\"rating\":"
            << engine.rating
            << ",\"rating_deviation\":"
            << engine.rating_deviation
            << ",\"search_config\":\""
            << escape_json(engine.search_config)
            << "\",\"seed\":"
            << engine.seed
            << ",\"selected_move\":";
        write_position_or_null(
            out,
            engine.selected_move);
        out << ",\"selected_move_legal\":"
            << (engine.selected_move_legal
                    ? "true"
                    : "false")
            << ",\"metrics\":";
        write_metrics(out, engine.metrics);

        out << ",\"candidates\":[";
        for (std::size_t c = 0;
             c < engine.candidates.size();
             ++c) {
            if (c) out << ',';
            const auto& candidate =
                engine.candidates[c];
            out << "{\"move\":";
            write_position_or_null(
                out,
                candidate.move);
            out << ",\"value\":";
            write_optional_double(
                out,
                candidate.value);
            out << ",\"policy\":";
            write_optional_double(
                out,
                candidate.policy);
            out << ",\"q\":";
            write_optional_double(
                out,
                candidate.q);
            out << ",\"exact\":"
                << (candidate.exact
                        ? "true"
                        : "false")
                << '}';
        }
        out << ']';

        out << ",\"diagnostics\":{";
        for (std::size_t d = 0;
             d < engine.diagnostics.size();
             ++d) {
            if (d) out << ',';
            out << '\"'
                << escape_json(
                    engine.diagnostics[d].key)
                << "\":\""
                << escape_json(
                    engine.diagnostics[d].value)
                << '\"';
        }
        out << "}}";
    }
    out << ']';

    out << ",\"disagreement\":{"
        << "\"move\":"
        << record.disagreement.move_disagreement
        << ",\"selected_value_stddev\":"
        << record.disagreement
               .selected_value_stddev
        << ",\"distinct_selected_moves\":"
        << record.disagreement
               .distinct_selected_moves
        << '}';

    out << ",\"exact\":{"
        << "\"attempted\":"
        << (record.exact.attempted
                ? "true"
                : "false")
        << ",\"completed\":"
        << (record.exact.completed
                ? "true"
                : "false")
        << ",\"budget_exhausted\":"
        << (record.exact.budget_exhausted
                ? "true"
                : "false")
        << ",\"nodes\":"
        << record.exact.nodes
        << ",\"selected_move\":";
    write_position_or_null(
        out,
        record.exact.selected_move);
    out << ",\"final_disc_difference\":";
    if (record.exact.final_disc_difference) {
        out << *record.exact.final_disc_difference;
    } else {
        out << "null";
    }
    out << ",\"candidates\":[";
    for (std::size_t i = 0;
         i < record.exact.candidates.size();
         ++i) {
        if (i) out << ',';
        const auto& candidate =
            record.exact.candidates[i];
        out << "{\"move\":";
        write_position_or_null(
            out,
            candidate.move);
        out << ",\"value\":";
        write_optional_double(
            out,
            candidate.value);
        out << ",\"exact\":true}";
    }
    out << "]}";

    out << ",\"after\":{\"selected_move\":";
    write_position_or_null(
        out,
        record.after.selected_move);
    out << ",\"value\":";
    write_optional_double(
        out,
        record.after.value);
    out << ",\"exact\":"
        << (record.after.exact
                ? "true"
                : "false")
        << ",\"source\":\""
        << escape_json(record.after.source)
        << "\"}";

    out << ",\"provenance\":[";
    for (std::size_t i = 0;
         i < record.provenance.size();
         ++i) {
        if (i) out << ',';
        out << '\"'
            << escape_json(
                record.provenance[i])
            << '\"';
    }
    out << "]}";
    return out.str();
}

void write_relabel_jsonl(
    const RelabelBatchResult& result,
    std::ostream& output) {
    for (const auto& record : result.records) {
        output << relabel_record_to_json(record)
               << '\n';
    }
}

DatasetEntry make_relabelled_dataset_entry(
    const DatasetEntry& parent,
    std::string dataset_id,
    std::string relabel_run_id,
    std::string created_at,
    std::string relabel_artifact,
    const RelabelBatchResult& result) {
    if (relabel_run_id.empty()) {
        throw std::invalid_argument(
            "relabel_run_id must not be empty");
    }
    if (result.records.empty()) {
        throw std::invalid_argument(
            "cannot create relabel Dataset from empty result");
    }

    std::vector<std::string> game_ids;
    for (const auto& record : result.records) {
        if (std::find(
                game_ids.begin(),
                game_ids.end(),
                record.game_id) ==
            game_ids.end()) {
            game_ids.push_back(record.game_id);
        }
    }

    DatasetEntry derived = derive_dataset(
        parent,
        std::move(dataset_id),
        "relabelled",
        parent.usage,
        std::move(created_at),
        std::move(game_ids));

    std::ostringstream history;
    history << "run=" << relabel_run_id
            << ";format=" << kRelabelRecordFormat
            << ";records=" << result.records.size()
            << ";engine_calls=" << result.engine_calls
            << ";exact_nodes=" << result.exact_nodes;
    derived.provenance.relabel_history.push_back(
        history.str());

    if (!relabel_artifact.empty() &&
        std::find(
            derived.artifacts.begin(),
            derived.artifacts.end(),
            relabel_artifact) ==
            derived.artifacts.end()) {
        derived.artifacts.push_back(
            std::move(relabel_artifact));
    }
    return derived;
}

}  // namespace kadoka::othello
