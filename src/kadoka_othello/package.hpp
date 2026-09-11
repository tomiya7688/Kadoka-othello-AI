#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace kadoka::othello {

enum class AIPackageInterface {
    Native,
    DynamicLibrary,
    ExternalProcess,
    Python,
    Network,
};

struct AIPackageManifest {
    std::string id;
    std::string name;
    std::string version;
    std::size_t protocol_version{1};
    std::vector<std::size_t> board_sizes;
    AIPackageInterface interface_type{AIPackageInterface::Native};
    std::string adapter{"pass_through"};
    std::string entry;
    std::vector<std::string> capabilities;
};

[[nodiscard]] AIPackageManifest load_ai_manifest(const std::string& path);
[[nodiscard]] std::string package_interface_name(AIPackageInterface type);

}  // namespace kadoka::othello
