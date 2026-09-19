#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kadoka::othello {

inline constexpr const char* kDatasetEntryFormat = "kadoka.dataset_entry.v1";
inline constexpr const char* kDatasetRecipeFormat = "kadoka.dataset_recipe.v1";

enum class DatasetUsage {
    Training,
    Validation,
    Benchmark,
    LeagueEvaluation,
};

struct DatasetModelRef {
    std::string model_id;
    std::string model_version;
};

struct DatasetProvenance {
    std::string source_type;
    std::size_t board_size{};
    std::optional<DatasetModelRef> generator_model;
    std::optional<DatasetModelRef> opponent_model;
    std::optional<std::string> parent_dataset;
    std::string search_config;
    std::vector<std::string> relabel_history;
    std::string created_at;
    std::string license;
    std::vector<std::string> game_ids;
};

struct DatasetEntry {
    std::string dataset_id;
    DatasetUsage usage{DatasetUsage::Training};
    DatasetProvenance provenance;
    std::vector<std::string> tags;
    std::vector<std::string> artifacts;
};

struct DatasetRecipeItem {
    std::string dataset_id;
    double ratio{};
};

struct DatasetRecipe {
    std::string recipe_id;
    std::string model_id;
    DatasetUsage usage{DatasetUsage::Training};
    std::vector<DatasetRecipeItem> datasets;
};

struct ResolvedDatasetSlice {
    std::string dataset_id;
    double ratio{};
    std::vector<std::string> game_ids;
};

struct ResolvedDatasetRecipe {
    std::string recipe_id;
    std::string model_id;
    DatasetUsage usage{DatasetUsage::Training};
    std::vector<ResolvedDatasetSlice> datasets;
    std::size_t unique_game_count{};
};

class DatasetRegistry {
public:
    void register_dataset(DatasetEntry entry);

    [[nodiscard]] const DatasetEntry* find(const std::string& dataset_id) const noexcept;
    [[nodiscard]] const std::vector<DatasetEntry>& entries() const noexcept;
    [[nodiscard]] ResolvedDatasetRecipe resolve(const DatasetRecipe& recipe) const;

private:
    std::vector<DatasetEntry> entries_;
};

[[nodiscard]] const char* dataset_usage_name(DatasetUsage usage) noexcept;
[[nodiscard]] DatasetUsage parse_dataset_usage(std::string_view value);

[[nodiscard]] std::string generate_dataset_id();

[[nodiscard]] DatasetEntry derive_dataset(
    const DatasetEntry& parent,
    std::string dataset_id,
    std::string derivation_tag,
    DatasetUsage usage,
    std::vector<std::string> game_ids);

[[nodiscard]] std::string dataset_entry_to_json(const DatasetEntry& entry);
[[nodiscard]] DatasetEntry parse_dataset_entry_json(std::string_view json);

[[nodiscard]] std::string dataset_recipe_to_json(const DatasetRecipe& recipe);
[[nodiscard]] DatasetRecipe parse_dataset_recipe_json(std::string_view json);

void write_dataset_registry_jsonl(
    const DatasetRegistry& registry,
    std::ostream& output);

[[nodiscard]] DatasetRegistry read_dataset_registry_jsonl(std::istream& input);

[[nodiscard]] std::string metadata_with_training_recipe(
    std::string_view metadata_json,
    const DatasetRecipe& recipe);

}  // namespace kadoka::othello
