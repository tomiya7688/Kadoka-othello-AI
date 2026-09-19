#include <cmath>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <vector>

#include "kadoka_othello/dataset_pool.hpp"
#include "test_support.hpp"

using namespace kadoka::othello;

namespace {

DatasetEntry make_entry(
    std::string dataset_id,
    DatasetUsage usage,
    std::size_t board_size,
    std::vector<std::string> game_ids) {
    DatasetEntry entry;
    entry.dataset_id = std::move(dataset_id);
    entry.usage = usage;
    entry.provenance.source_type = "league";
    entry.provenance.board_size = board_size;
    entry.provenance.generator_model =
        DatasetModelRef{"kadoka.generator", "1.2.3"};
    entry.provenance.opponent_model =
        DatasetModelRef{"kadoka.opponent", "2.0.0"};
    entry.provenance.search_config = "depth=7;nodes=10000";
    entry.provenance.relabel_history = {"initial-label"};
    entry.provenance.created_at = "2026-09-20T00:00:00Z";
    entry.provenance.license = "MIT";
    entry.provenance.game_ids = std::move(game_ids);
    entry.tags = {"league"};
    entry.artifacts = {
        "board-state.jsonl",
        "game-aux.jsonl",
    };
    return entry;
}

template <typename Callable>
bool throws_invalid_argument(const Callable& callable) {
    try {
        callable();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void test_dataset_id() {
    const std::string first = generate_dataset_id();
    const std::string second = generate_dataset_id();
    KADOKA_REQUIRE(first.rfind("dataset-", 0) == 0);
    KADOKA_REQUIRE(first.size() == 34);
    KADOKA_REQUIRE(first != second);
}

void test_registry_round_trip_and_board_sizes() {
    DatasetRegistry registry;
    registry.register_dataset(make_entry(
        "league-6", DatasetUsage::Training, 6, {"game-6"}));
    registry.register_dataset(make_entry(
        "league-8", DatasetUsage::Training, 8, {"game-8"}));
    registry.register_dataset(make_entry(
        "league-10", DatasetUsage::Benchmark, 10, {"game-10"}));

    std::ostringstream output;
    write_dataset_registry_jsonl(registry, output);

    std::istringstream input(output.str());
    DatasetRegistry parsed = read_dataset_registry_jsonl(input);
    KADOKA_REQUIRE(parsed.entries().size() == 3);
    KADOKA_REQUIRE(parsed.find("league-6") != nullptr);
    KADOKA_REQUIRE(parsed.find("league-8") != nullptr);
    KADOKA_REQUIRE(parsed.find("league-10") != nullptr);
    KADOKA_REQUIRE(
        parsed.find("league-10")->usage == DatasetUsage::Benchmark);
    KADOKA_REQUIRE(
        parsed.find("league-8")->provenance.generator_model->model_version ==
        "1.2.3");

    KADOKA_REQUIRE(throws_invalid_argument([&] {
        parsed.register_dataset(make_entry(
            "league-8", DatasetUsage::Training, 8, {"another-game"}));
    }));
}

void test_derived_dataset() {
    const DatasetEntry parent = make_entry(
        "raw-league",
        DatasetUsage::Training,
        8,
        {"game-a", "game-b", "game-c"});

    const DatasetEntry derived = derive_dataset(
        parent,
        "opening-only",
        "opening_only",
        DatasetUsage::Training,
        "2026-09-20T01:00:00Z",
        {"game-a", "game-c"});

    KADOKA_REQUIRE(derived.dataset_id == "opening-only");
    KADOKA_REQUIRE(derived.provenance.source_type == "derived");
    KADOKA_REQUIRE(
        derived.provenance.parent_dataset ==
        std::optional<std::string>{"raw-league"});
    KADOKA_REQUIRE(derived.provenance.board_size == 8);
    KADOKA_REQUIRE(
        derived.provenance.created_at == "2026-09-20T01:00:00Z");
    KADOKA_REQUIRE(derived.provenance.game_ids.size() == 2);
    KADOKA_REQUIRE(derived.tags.back() == "opening_only");

    KADOKA_REQUIRE(throws_invalid_argument([&] {
        static_cast<void>(derive_dataset(
            parent,
            "bad-derived",
            "endgame_only",
            DatasetUsage::Training,
            "2026-09-20T02:00:00Z",
            {"not-in-parent"}));
    }));
}

void test_provenance_parent_validation() {
    DatasetRegistry missing_parent;
    DatasetEntry orphan = make_entry(
        "orphan",
        DatasetUsage::Training,
        8,
        {"game-a"});
    orphan.provenance.source_type = "derived";
    orphan.provenance.parent_dataset = "missing";
    missing_parent.register_dataset(orphan);
    KADOKA_REQUIRE(throws_invalid_argument([&] {
        missing_parent.validate();
    }));

    DatasetRegistry cycle;
    DatasetEntry first = make_entry(
        "cycle-a",
        DatasetUsage::Training,
        8,
        {"game-cycle"});
    DatasetEntry second = make_entry(
        "cycle-b",
        DatasetUsage::Training,
        8,
        {"game-cycle"});
    first.provenance.source_type = "derived";
    second.provenance.source_type = "derived";
    first.provenance.parent_dataset = "cycle-b";
    second.provenance.parent_dataset = "cycle-a";
    cycle.register_dataset(first);
    cycle.register_dataset(second);
    KADOKA_REQUIRE(throws_invalid_argument([&] {
        cycle.validate();
    }));
}

void test_recipe_dedup_and_usage_separation() {
    DatasetRegistry registry;
    const DatasetEntry raw = make_entry(
        "raw",
        DatasetUsage::Training,
        8,
        {"game-a", "game-b"});
    const DatasetEntry derived = derive_dataset(
        raw,
        "derived",
        "uncertain_positions",
        DatasetUsage::Training,
        "2026-09-20T01:00:00Z",
        {"game-b"});
    registry.register_dataset(raw);
    registry.register_dataset(derived);
    registry.register_dataset(make_entry(
        "fixed-validation",
        DatasetUsage::Validation,
        8,
        {"game-v"}));

    DatasetRecipe recipe;
    recipe.recipe_id = "recipe-eval-standard-v1";
    recipe.model_id = "eval_ml_standard";
    recipe.usage = DatasetUsage::Training;
    recipe.datasets = {
        {"derived", 0.25},
        {"raw", 0.75},
    };

    const ResolvedDatasetRecipe resolved = registry.resolve(recipe);
    KADOKA_REQUIRE(resolved.datasets.size() == 2);
    KADOKA_REQUIRE(resolved.unique_game_count == 2);
    KADOKA_REQUIRE(resolved.datasets[0].game_ids.size() == 1);
    KADOKA_REQUIRE(resolved.datasets[0].game_ids[0] == "game-b");
    KADOKA_REQUIRE(resolved.datasets[1].game_ids.size() == 1);
    KADOKA_REQUIRE(resolved.datasets[1].game_ids[0] == "game-a");
    KADOKA_REQUIRE(
        std::abs(
            resolved.datasets[0].ratio +
            resolved.datasets[1].ratio - 1.0) < 1e-12);

    std::unordered_set<std::string> unique_games;
    for (const auto& slice : resolved.datasets) {
        for (const auto& game_id : slice.game_ids) {
            KADOKA_REQUIRE(unique_games.insert(game_id).second);
        }
    }

    DatasetRecipe invalid = recipe;
    invalid.datasets = {{"fixed-validation", 1.0}};
    KADOKA_REQUIRE(throws_invalid_argument([&] {
        static_cast<void>(registry.resolve(invalid));
    }));
}

void test_recipe_round_trip_and_model_metadata() {
    DatasetRecipe recipe;
    recipe.recipe_id = "recipe-1";
    recipe.model_id = "kadoka.eval_ml";
    recipe.usage = DatasetUsage::Training;
    recipe.datasets = {
        {"league-8", 0.6},
        {"exact-endgame", 0.4},
    };

    const std::string json = dataset_recipe_to_json(recipe);
    const DatasetRecipe parsed = parse_dataset_recipe_json(json);
    KADOKA_REQUIRE(parsed.recipe_id == recipe.recipe_id);
    KADOKA_REQUIRE(parsed.model_id == recipe.model_id);
    KADOKA_REQUIRE(parsed.datasets.size() == 2);
    KADOKA_REQUIRE(std::abs(parsed.datasets[0].ratio - 0.6) < 1e-12);

    const std::string metadata =
        "{\"format\":\"kadoka.ai_metadata.v1\","
        "\"model_id\":\"kadoka.eval_ml\","
        "\"model_name\":\"Eval ML\","
        "\"model_version\":\"1.0.0\","
        "\"architecture\":\"eval_ml\","
        "\"game\":\"othello\","
        "\"license\":\"MIT\"}";

    const std::string with_recipe =
        metadata_with_training_recipe(metadata, recipe);
    KADOKA_REQUIRE(
        with_recipe.find(
            "\"training_recipe\":{\"format\":\"kadoka.dataset_recipe.v1\"") !=
        std::string::npos);
    KADOKA_REQUIRE(
        with_recipe.find("\"recipe_id\":\"recipe-1\"") !=
        std::string::npos);

    KADOKA_REQUIRE(throws_invalid_argument([&] {
        static_cast<void>(
            metadata_with_training_recipe(with_recipe, recipe));
    }));

    DatasetRecipe wrong_model = recipe;
    wrong_model.model_id = "other.model";
    KADOKA_REQUIRE(throws_invalid_argument([&] {
        static_cast<void>(
            metadata_with_training_recipe(metadata, wrong_model));
    }));

    DatasetRecipe validation_recipe = recipe;
    validation_recipe.usage = DatasetUsage::Validation;
    KADOKA_REQUIRE(throws_invalid_argument([&] {
        static_cast<void>(
            metadata_with_training_recipe(metadata, validation_recipe));
    }));
}

void test_entry_json_unknown_optional_field() {
    const DatasetEntry entry = make_entry(
        "raw-8",
        DatasetUsage::LeagueEvaluation,
        8,
        {"game-x"});
    std::string json = dataset_entry_to_json(entry);
    json.insert(
        json.size() - 1,
        ",\"future_optional\":{\"enabled\":true}");

    const DatasetEntry parsed = parse_dataset_entry_json(json);
    KADOKA_REQUIRE(parsed.dataset_id == "raw-8");
    KADOKA_REQUIRE(
        parsed.usage == DatasetUsage::LeagueEvaluation);
    KADOKA_REQUIRE(parsed.provenance.game_ids[0] == "game-x");
}

}  // namespace

int main() {
    test_dataset_id();
    test_registry_round_trip_and_board_sizes();
    test_derived_dataset();
    test_provenance_parent_validation();
    test_recipe_dedup_and_usage_separation();
    test_recipe_round_trip_and_model_metadata();
    test_entry_json_unknown_optional_field();
    return 0;
}
