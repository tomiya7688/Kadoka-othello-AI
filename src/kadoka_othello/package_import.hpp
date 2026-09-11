#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include "kadoka_othello/package.hpp"

namespace kadoka::othello {

struct AIPackageImportRequest {
    std::string source_path;
    std::string destination_directory;
    std::string id;
    std::string name;
    std::string version{"0.1.0"};
    AIPackageInterface interface_type{AIPackageInterface::ExternalProcess};
    std::string adapter{"pass_through"};
    std::vector<std::size_t> board_sizes{6, 8, 10};
    bool copy_source{true};
};

[[nodiscard]] std::string import_ai_package(const AIPackageImportRequest& request);

}  // namespace kadoka::othello
