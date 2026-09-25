#include <algorithm>
#include <cstdint>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "kadoka_othello/confidence.hpp"
#include "kadoka_othello/game.hpp"
#include "kadoka_othello/game_record.hpp"
#include "kadoka_othello/relabel.hpp"
#include "test_support.hpp"

using namespace kadoka::othello;

namespace {

RelabelInput make_initial_input(
    std::size_t board_size,
    std::string game_id,
    std::string source_dataset_id) {
    Game game(board_size);

    BoardStateRecord board_state;
    board_state.game_id = game_id;
    board_state.ply = 0;
    board_state.state = make_core_state(CoreStateView{
        &game.board(),
        game.current_player(),
        {},
    });

    RelabelOriginalLabel original;
    const auto legal = game.legal_moves();
    if (!legal.empty()) {
        original.selected_move = legal.back();
    }
    original.value = -0.25;
    original.source = "obake_generated_label";

    RelabelInput input;
    input.position = make_relabel_position(
        board_state,
        original,
        std::move(source_dataset_id));
    input.confidence.game_id = board_state.game_id;
    input.confidence.ply = board_state.ply;
    input.confidence.board_size = board_size;
    input.confidence.factors.set(
        confidence_factor::kSourceRatingDeviation,
        300.0);
    input.confidence.factors.set(
        confidence_factor::kMultiAiMoveDisagreement,
        1.0);
    input.confidence.factors.set(
        confidence_factor::kTopCandidateGap,
        0.01);
    input.confidence.factors.set(
        confidence_factor::kEmptyCount,
        static_cast<double>(
            board_state.state.cells.size() - 4));
    return input;
}

RelabelInput make_late_6x6_input() {
    Game game(6);
    CoreState chosen;
    std::size_t chosen_ply = 0;
    bool found = false;

    while (game.status() == GameStatus::Playing) {
        const std::size_t empty =
            game.board().count(Cell::Empty);
        if (empty <= 8 &&
            !game.can_pass() &&
            !game.legal_moves().empty()) {
            chosen = make_core_state(CoreStateView{
                &game.board(),
                game.current_player(),
                {},
            });
            chosen_ply = game.ply();
            found = true;
        }

        if (game.can_pass()) {
            KADOKA_REQUIRE(game.pass());
            continue;
        }
        const auto legal = game.legal_moves();
        if (legal.empty()) break;
        KADOKA_REQUIRE(game.play(legal.front()));
    }

    KADOKA_REQUIRE(found);

    BoardStateRecord board_state;
    board_state.game_id =
        "01ARZ3NDEKTSV4RRFFQ69G5FAX";
    board_state.ply = chosen_ply;
    board_state.state = chosen;

    RelabelInput input;
    input.position = make_relabel_position(
        board_state,
        {},
        "obake-late-game");
    input.confidence.game_id = board_state.game_id;
    input.confidence.ply = board_state.ply;
    input.confidence.board_size = 6;
    input.confidence.factors.set(
        confidence_factor::kEmptyCount,
        static_cast<double>(
            std::count(
                chosen.cells.begin(),
                chosen.cells.end(),
                Cell::Empty)));
    input.confidence.factors.set(
        confidence_factor::kExactEndgameAvailable,
        1.0);
    input.confidence.factors.set(
        confidence_factor::kMultiAiMoveDisagreement,
        1.0);
    return input;
}

RelabelEngineSpec make_engine(
    std::string manifest_path,
    std::uint64_t seed,
    double rating,
    double rd,
    std::string search_config) {
    RelabelEngineSpec spec;
    spec.manifest_path = std::move(manifest_path);
    spec.seed = seed;
    spec.rating = rating;
    spec.rating_deviation = rd;
    spec.search_config = std::move(search_config);
    return spec;
}

void test_multi_engine_preserves_provenance(
    const std::string& random_manifest,
    const std::string& obake_manifest) {
    BaselineConfidenceCalculator calculator;
    const auto confidence_parameters =
        default_confidence_board_parameters();
    const auto relabel_parameters =
        default_relabel_board_parameters();

    const RelabelInput input = make_initial_input(
        8,
        "01ARZ3NDEKTSV4RRFFQ69G5FAY",
        "obake-raw-dataset");

    RelabelBudget budget;
    budget.max_positions = 1;
    budget.max_engine_calls = 2;
    budget.max_elapsed_ms = 10000;
    budget.enable_exact_endgame = false;
    budget.candidate_mode =
        RelabelCandidateMode::AllLegal;

    const RelabelBatchResult result = relabel_positions(
        {input},
        {
            make_engine(
                random_manifest,
                1001,
                1800.0,
                60.0,
                "mode=random-control"),
            make_engine(
                obake_manifest,
                1002,
                1200.0,
                180.0,
                "mode=character-reference"),
        },
        calculator,
        confidence_parameters,
        relabel_parameters,
        budget,
        777);

    KADOKA_REQUIRE(result.records.size() == 1);
    KADOKA_REQUIRE(result.engine_calls == 2);

    const RelabelRecord& record = result.records.front();
    KADOKA_REQUIRE(
        record.source_dataset_id ==
        "obake-raw-dataset");
    KADOKA_REQUIRE(
        record.before.source ==
        "obake_generated_label");
    KADOKA_REQUIRE(record.engines.size() == 2);
    KADOKA_REQUIRE(
        record.engines[0].rating == 1800.0);
    KADOKA_REQUIRE(
        record.engines[0].rating_deviation == 60.0);
    KADOKA_REQUIRE(
        record.engines[0].search_config ==
        "mode=random-control");
    KADOKA_REQUIRE(
        !record.engines[0].model_version.empty());
    KADOKA_REQUIRE(
        record.engines[0].candidates.size() ==
        record.legal_moves.size());
    KADOKA_REQUIRE(
        record.engines[1].candidates.size() ==
        record.legal_moves.size());

    KADOKA_REQUIRE(
        record.disagreement.move_disagreement >= 0.0);
    KADOKA_REQUIRE(
        record.disagreement.move_disagreement <= 1.0);
    KADOKA_REQUIRE(
        record.after.selected_move.has_value());
    KADOKA_REQUIRE(!record.after.exact);
    KADOKA_REQUIRE(
        record.after.source.find(
            "engine:kadoka.random@") == 0);

    const std::string json =
        relabel_record_to_json(record);
    KADOKA_REQUIRE(
        json.find(
            "\"format\":\"kadoka.relabel_record.v1\"") !=
        std::string::npos);
    KADOKA_REQUIRE(
        json.find("\"before\"") != std::string::npos);
    KADOKA_REQUIRE(
        json.find("\"after\"") != std::string::npos);
    KADOKA_REQUIRE(
        json.find("\"rating_deviation\":60") !=
        std::string::npos);
    KADOKA_REQUIRE(
        json.find("mode=random-control") !=
        std::string::npos);
    KADOKA_REQUIRE(
        json.find("\"disagreement\"") !=
        std::string::npos);
}

void test_top_k_and_engine_budget(
    const std::string& random_manifest,
    const std::string& obake_manifest) {
    BaselineConfidenceCalculator calculator;
    const RelabelInput input = make_initial_input(
        8,
        "01ARZ3NDEKTSV4RRFFQ69G5FAZ",
        "top-k-source");

    RelabelBudget budget;
    budget.max_positions = 1;
    budget.max_engine_calls = 1;
    budget.max_elapsed_ms = 10000;
    budget.enable_exact_endgame = false;
    budget.candidate_mode =
        RelabelCandidateMode::TopK;
    budget.top_k = 2;

    const auto result = relabel_positions(
        {input},
        {
            make_engine(
                random_manifest,
                1,
                1700.0,
                50.0,
                "depth=0"),
            make_engine(
                obake_manifest,
                2,
                1300.0,
                200.0,
                "character"),
        },
        calculator,
        default_confidence_board_parameters(),
        default_relabel_board_parameters(),
        budget,
        123);

    KADOKA_REQUIRE(result.records.size() == 1);
    KADOKA_REQUIRE(result.engine_calls == 1);
    KADOKA_REQUIRE(result.engine_budget_exhausted);
    KADOKA_REQUIRE(
        result.records[0].engines.size() == 1);
    KADOKA_REQUIRE(
        result.records[0].engines[0].
            candidates.size() <= 2);
}

void test_exact_endgame_and_node_budget() {
    BaselineConfidenceCalculator calculator;
    const RelabelInput input = make_late_6x6_input();

    RelabelBudget complete_budget;
    complete_budget.max_positions = 1;
    complete_budget.max_engine_calls = 0;
    complete_budget.max_exact_nodes = 2000000;
    complete_budget.max_elapsed_ms = 10000;
    complete_budget.enable_exact_endgame = true;

    const auto complete = relabel_positions(
        {input},
        {},
        calculator,
        default_confidence_board_parameters(),
        default_relabel_board_parameters(),
        complete_budget,
        7);

    KADOKA_REQUIRE(complete.records.size() == 1);
    const auto& record = complete.records.front();
    KADOKA_REQUIRE(record.exact.attempted);
    KADOKA_REQUIRE(record.exact.completed);
    KADOKA_REQUIRE(
        record.exact.candidates.size() ==
        record.legal_moves.size());
    KADOKA_REQUIRE(
        record.exact.final_disc_difference.has_value());
    KADOKA_REQUIRE(record.after.exact);
    KADOKA_REQUIRE(
        record.after.source == "exact_endgame");

    for (const auto& candidate :
         record.exact.candidates) {
        KADOKA_REQUIRE(candidate.exact);
        KADOKA_REQUIRE(candidate.value.has_value());
    }

    RelabelBudget tiny_budget = complete_budget;
    tiny_budget.max_exact_nodes = 1;

    const auto limited = relabel_positions(
        {input},
        {},
        calculator,
        default_confidence_board_parameters(),
        default_relabel_board_parameters(),
        tiny_budget,
        7);

    KADOKA_REQUIRE(limited.records.size() == 1);
    KADOKA_REQUIRE(
        limited.records[0].exact.attempted);
    KADOKA_REQUIRE(
        !limited.records[0].exact.completed);
    KADOKA_REQUIRE(
        limited.records[0].exact.budget_exhausted);
    KADOKA_REQUIRE(
        limited.exact_node_budget_exhausted);
}

void test_6_8_10_common_interface(
    const std::string& random_manifest) {
    BaselineConfidenceCalculator calculator;
    std::vector<RelabelInput> inputs;
    inputs.push_back(make_initial_input(
        6,
        "01ARZ3NDEKTSV4RRFFQ69G5FB0",
        "size6"));
    inputs.push_back(make_initial_input(
        8,
        "01ARZ3NDEKTSV4RRFFQ69G5FB1",
        "size8"));
    inputs.push_back(make_initial_input(
        10,
        "01ARZ3NDEKTSV4RRFFQ69G5FB2",
        "size10"));

    RelabelBudget budget;
    budget.max_positions = 3;
    budget.max_engine_calls = 3;
    budget.max_elapsed_ms = 10000;
    budget.enable_exact_endgame = false;

    const auto result = relabel_positions(
        inputs,
        {
            make_engine(
                random_manifest,
                404,
                1500.0,
                350.0,
                "random"),
        },
        calculator,
        default_confidence_board_parameters(),
        default_relabel_board_parameters(),
        budget,
        909);

    KADOKA_REQUIRE(result.records.size() == 3);
    bool found6 = false;
    bool found8 = false;
    bool found10 = false;
    for (const auto& record : result.records) {
        if (record.board_size == 6) found6 = true;
        if (record.board_size == 8) found8 = true;
        if (record.board_size == 10) found10 = true;
        KADOKA_REQUIRE(record.engines.size() == 1);
        KADOKA_REQUIRE(
            record.engines[0].selected_move_legal);
        KADOKA_REQUIRE(
            record.board_analysis.legal_move_count ==
            record.legal_moves.size());
    }
    KADOKA_REQUIRE(found6);
    KADOKA_REQUIRE(found8);
    KADOKA_REQUIRE(found10);
}

}  // namespace

int main(int argc, char** argv) {
    KADOKA_REQUIRE(argc == 3);
    const std::string random_manifest = argv[1];
    const std::string obake_manifest = argv[2];

    test_multi_engine_preserves_provenance(
        random_manifest,
        obake_manifest);
    test_top_k_and_engine_budget(
        random_manifest,
        obake_manifest);
    test_exact_endgame_and_node_budget();
    test_6_8_10_common_interface(
        random_manifest);
    return 0;
}
