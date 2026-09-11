#pragma once

#include <string>
#include <vector>

namespace kadoka::othello {

struct ModelAssetDescriptor {
    std::string id;
    std::string type;
    std::string path;
    bool required{true};
};

struct ModelRootDescriptor {
    std::string format{"kadoka.model.v1"};
    std::string model_id;
    std::string model_kind;
    std::string engine;
    std::vector<ModelAssetDescriptor> assets;
    std::string source_directory;
};

[[nodiscard]] ModelRootDescriptor load_model_root_descriptor(const std::string& path);
[[nodiscard]] std::string find_model_asset_path(
    const ModelRootDescriptor& descriptor,
    const std::string& asset_id);

}  // namespace kadoka::othello
