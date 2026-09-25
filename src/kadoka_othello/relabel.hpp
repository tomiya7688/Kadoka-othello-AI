#pragma once

#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

#include "kadoka_othello/ai.hpp"
#include "kadoka_othello/confidence.hpp"
#include "kadoka_othello/core_state.hpp"
#include "kadoka_othello/game_record.hpp"

namespace kadoka::othello {

inline constexpr const char* kRelabelRecordFormat =
    "kadoka.relabel_record.v1";

enum class RelabelCandidateMode {
    AllLegal,
    TopK,
};

struct RelabelBoardParameters {
    std::size_t board_size{8};
    std::size_t exact_endgame_empty_threshold{12};
};

struct RelabelBudget {
    std::size_t max_positions{100};
    std::size_t max_engine_calls{1000};
    std::uint64_t max_exact_nodes{1000000};
    std::uint64_t max_elapsed_ms{30000};
    RelabelCandidateMode candidate_mode{RelabelCandidateMode::AllLegal};
    std::size_t top_k{8};
    bool enable_exact_endgame{true};
};

struct RelabelEngineSpec {
    std::string manifest_path;
    std::uint64_t seed{1};
    double rating{1500.0};
    double rating_deviation{350.0};
    std::string search_config;
};

struct RelabelOriginalLabel {
    std::optional<Position> selected_move;
    std::optional<double> value;
    std::string source;
};

struct RelabelPosition {
    std::string game_id;
    std::size_t ply{};
    CoreState state;
    RelabelOriginalLabel original_label;
    std::string source_dataset_id;
};

struct RelabelCandidateEvaluation {
    Position move{};
    std::optional<double> value;
    std::optional<double> policy;
    std::optional<double> q;
    bool exact{false};
};

struct RelabelEngineResult {
    std::string model_id;
    std::string model_version;
    double rating{1500.0};
    double rating_deviation{350.0};
    std::string search_config;
    std::uint64_t seed{};
    Position selected_move{};
    AIMoveMetrics metrics;
    std::vector<RelabelCandidateEvaluation> candidates;
    std::vector<AIDiagnostic> diagnostics;
};

struct RelabelExactResult {
    bool attempted{false};
    bool completed{false};
    bool budget_exhausted{false};
    std::uint64_t nodes{};
    std::optional<Position> selected_move;
    std::optional<int> final_disc_difference;
    std::vector<RelabelCandidateEvaluation> candidates;
};

struct RelabelBoardAnalysis {
    std::size_t legal_move_count{};
    std::size_t opponent_legal_move_count{};
    std::size_t frontier_self{};
    std::size_t frontier_opponent{};
    std::size_t stable_corner_self{};
    std::size_t stable_corner_opponent{};
    std::size_t empty_count{};
    int mobility_difference{};
    int frontier_difference{};
    int stable_corner_difference{};
    int corner_availability_difference{};
    int parity{};
};

struct RelabelDisagreement {
    double move_disagreement{};
    double selected_value_stddev{};
    std::size_t distinct_selected_moves{};
};

struct RelabelDerivedLabel {
    std::optional<Position> selected_move;
    std::optional<double> value;
    bool exact{false};
    std::string source;
};

struct RelabelRecord {
    std::string game_id;
    std::size_t ply{};
    std::size_t board_size{};
    Player side_to_move{Player::Black};
    std::string source_dataset_id;
    RelabelOriginalLabel before;
    ConfidenceEstimate confidence_before;
    RelabelBoardAnalysis board_analysis;
    std::vector<Position> legal_moves;
    std::vector<RelabelEngineResult> engines;
    RelabelDisagreement disagreement;
    RelabelExactResult exact;
    RelabelDerivedLabel after;
    std::vector<std::string> provenance;
};

struct RelabelInput {
    RelabelPosition position;
    ConfidenceSample confidence;
};

struct RelabelBatchResult {
    std::vector<RelabelRecord> records;
    std::size_t selected_positions{};
    std::size_t engine_calls{};
    std::uint64_t exact_nodes{};
    bool engine_budget_exhausted{false};
    bool elapsed_budget_exhausted{false};
    bool exact_node_budget_exhausted{false};
};

[[nodiscard]] RelabelPosition make_relabel_position(
    const BoardStateRecord& state,
    RelabelOriginalLabel original_label = {},
    std::string source_dataset_id = {});

[[nodiscard]] std::vector<RelabelBoardParameters>
default_relabel_board_parameters();

[[nodiscard]] RelabelBoardAnalysis analyze_relabel_position(
    const CoreState& state);

[[nodiscard]] RelabelBatchResult relabel_positions(
    const std::vector<RelabelInput>& inputs,
    const std::vector<RelabelEngineSpec>& engines,
    const IConfidenceCalculator& confidence_calculator,
    const std::vector<ConfidenceBoardParameters>& confidence_parameters,
    const std::vector<RelabelBoardParameters>& relabel_parameters,
    const RelabelBudget& budget,
    std::uint64_t sampler_seed);

[[nodiscard]] std::string relabel_record_to_json(
    const RelabelRecord& record);

void write_relabel_jsonl(
    const RelabelBatchResult& result,
    std::ostream& output);

}  // namespace kadoka::othello
