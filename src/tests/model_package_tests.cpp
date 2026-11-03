#include "kadoka_othello/evaluator_ai.hpp"
#include "kadoka_othello/model_descriptor.hpp"
#include "kadoka_othello/obake_kadoka.hpp"
#include "test_support.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <system_error>

using namespace kadoka::othello;

namespace {

class TemporaryDirectory {
  public:
    TemporaryDirectory() {
        const auto timestamp = std::chrono::steady_clock::now().time_since_epoch().count();
        path_ = std::filesystem::temp_directory_path() / ("kadoka_model_package_tests_" + std::to_string(timestamp));
        std::filesystem::create_directories(path_);
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& path() const noexcept {
        return path_;
    }

  private:
    std::filesystem::path path_;
};

void write_file(const std::filesystem::path& path, const std::string& content) {
    std::ofstream output(path, std::ios::binary);
    if (!output)
        throw std::runtime_error("failed to create test model file");
    output << content;
    if (!output)
        throw std::runtime_error("failed to write test model file");
}

void expect_load_failure(const std::filesystem::path& model_path) {
    bool failed = false;
    try {
        static_cast<void>(load_model_root_descriptor(model_path.string()));
    } catch (const std::runtime_error&) {
        failed = true;
    }
    KADOKA_REQUIRE(failed);
}

std::string descriptor_with_assets(const std::string& assets) {
    return "{"
           "\"format\":\"kadoka.model.v1\","
           "\"model_id\":\"test.model\","
           "\"model_kind\":\"test\","
           "\"engine\":\"test.engine\","
           "\"assets\":" +
           assets + "}";
}

void test_valid_descriptor_and_optional_unknown_asset() {
    TemporaryDirectory directory;
    write_file(directory.path() / "evaluator.json", "{}");
    const auto model_path = directory.path() / "model.json";
    write_file(model_path, descriptor_with_assets("["
                                                  "{\"id\":\"evaluator\",\"type\":\"kadoka.script_evaluator.v1\","
                                                  "\"path\":\"evaluator.json\",\"required\":true},"
                                                  "{\"id\":\"future\",\"type\":\"kadoka.future_asset.v2\","
                                                  "\"path\":\"future.bin\",\"required\":false}"
                                                  "]"));

    const ModelRootDescriptor model = load_model_root_descriptor(model_path.string());
    KADOKA_REQUIRE(model.format == "kadoka.model.v1");
    KADOKA_REQUIRE(model.assets.size() == 2U);
    KADOKA_REQUIRE(find_model_asset(model, "future") != nullptr);
    KADOKA_REQUIRE(std::filesystem::path(find_model_asset_path(model, "evaluator")) ==
                   std::filesystem::absolute(directory.path() / "evaluator.json"));
}

void test_required_asset_must_exist() {
    TemporaryDirectory directory;
    const auto model_path = directory.path() / "model.json";
    write_file(model_path, descriptor_with_assets("[{\"id\":\"evaluator\",\"type\":\"kadoka.script_evaluator.v1\","
                                                  "\"path\":\"missing.json\",\"required\":true}]"));
    expect_load_failure(model_path);
}

void test_unknown_required_asset_type_fails() {
    TemporaryDirectory directory;
    const auto model_path = directory.path() / "model.json";
    write_file(model_path, descriptor_with_assets("[{\"id\":\"future\",\"type\":\"kadoka.future_asset.v2\","
                                                  "\"path\":\"future.bin\",\"required\":true}]"));
    expect_load_failure(model_path);
}

void test_duplicate_asset_id_fails() {
    TemporaryDirectory directory;
    write_file(directory.path() / "evaluator.json", "{}");
    const auto model_path = directory.path() / "model.json";
    write_file(model_path, descriptor_with_assets("["
                                                  "{\"id\":\"same\",\"type\":\"kadoka.script_evaluator.v1\","
                                                  "\"path\":\"evaluator.json\",\"required\":true},"
                                                  "{\"id\":\"same\",\"type\":\"kadoka.script_evaluator.v1\","
                                                  "\"path\":\"evaluator.json\",\"required\":true}"
                                                  "]"));
    expect_load_failure(model_path);
}

void test_invalid_format_and_json_fail() {
    TemporaryDirectory directory;
    const auto model_path = directory.path() / "model.json";
    write_file(model_path, "{\"format\":\"kadoka.model.v2\",\"model_id\":\"test\",\"model_kind\":\"test\","
                           "\"engine\":\"test.engine\",\"assets\":[]}");
    expect_load_failure(model_path);

    write_file(model_path, "{\"format\":\"kadoka.model.v1\",\"model_id\":\"test\",\"model_kind\":\"test\","
                           "\"engine\":\"test.engine\",\"assets\":[]} trailing");
    expect_load_failure(model_path);
}

void test_non_json_whitespace_fails() {
    TemporaryDirectory directory;
    const auto model_path = directory.path() / "model.json";
    write_file(model_path, "{\v\"format\":\"kadoka.model.v1\",\"model_id\":\"test\",\"model_kind\":\"test\","
                           "\"engine\":\"test.engine\",\"assets\":[]}");
    expect_load_failure(model_path);

    write_file(model_path, "{\"format\":\"kadoka.model.v1\",\f\"model_id\":\"test\",\"model_kind\":\"test\","
                           "\"engine\":\"test.engine\",\"assets\":[]}");
    expect_load_failure(model_path);
}

void test_control_z_does_not_hide_trailing_data() {
    TemporaryDirectory directory;
    const auto model_path = directory.path() / "model.json";
    write_file(model_path, "{\"format\":\"kadoka.model.v1\",\"model_id\":\"test\",\"model_kind\":\"test\","
                           "\"engine\":\"test.engine\",\"assets\":[]}\x1A trailing");
    expect_load_failure(model_path);
}

void test_invalid_utf8_fails() {
    TemporaryDirectory directory;
    const auto model_path = directory.path() / "model.json";
    const std::string suffix = "\",\"model_kind\":\"test\",\"engine\":\"test.engine\",\"assets\":[]}";

    std::string overlong_encoding;
    overlong_encoding.push_back(static_cast<char>(0xc0));
    overlong_encoding.push_back(static_cast<char>(0xaf));
    write_file(model_path, "{\"format\":\"kadoka.model.v1\",\"model_id\":\"" + overlong_encoding + suffix);
    expect_load_failure(model_path);

    std::string invalid_continuation;
    invalid_continuation.push_back(static_cast<char>(0xc3));
    invalid_continuation.push_back('(');
    write_file(model_path, "{\"format\":\"kadoka.model.v1\",\"model_id\":\"" + invalid_continuation + suffix);
    expect_load_failure(model_path);

    std::string surrogate_encoding;
    surrogate_encoding.push_back(static_cast<char>(0xed));
    surrogate_encoding.push_back(static_cast<char>(0xa0));
    surrogate_encoding.push_back(static_cast<char>(0x80));
    write_file(model_path, "{\"format\":\"kadoka.model.v1\",\"model_id\":\"" + surrogate_encoding + suffix);
    expect_load_failure(model_path);
}

void test_asset_required_field_is_mandatory() {
    TemporaryDirectory directory;
    write_file(directory.path() / "evaluator.json", "{}");
    const auto model_path = directory.path() / "model.json";
    write_file(model_path, descriptor_with_assets("[{\"id\":\"evaluator\",\"type\":\"kadoka.script_evaluator.v1\","
                                                  "\"path\":\"evaluator.json\"}]"));
    expect_load_failure(model_path);
}

void test_obake_engine_rejects_mismatched_asset_type() {
    TemporaryDirectory directory;
    write_file(directory.path() / "evaluator.json", "{}");
    write_file(directory.path() / "behavior.json", "{}");
    const auto model_path = directory.path() / "model.json";
    write_file(model_path, "{"
                           "\"format\":\"kadoka.model.v1\","
                           "\"model_id\":\"kadoka.obake_kadoka\","
                           "\"model_kind\":\"character_evaluator_randomizer\","
                           "\"engine\":\"builtin:kadoka.obake_kadoka\","
                           "\"assets\":["
                           "{\"id\":\"evaluator\",\"type\":\"kadoka.script_evaluator.v1\","
                           "\"path\":\"evaluator.json\",\"required\":true},"
                           "{\"id\":\"behavior\",\"type\":\"kadoka.character_behavior.v1\","
                           "\"path\":\"behavior.json\",\"required\":true}"
                           "]"
                           "}");

    bool failed = false;
    try {
        static_cast<void>(load_obake_kadoka_model(model_path.string()));
    } catch (const std::runtime_error&) {
        failed = true;
    }
    KADOKA_REQUIRE(failed);
}

void test_obake_engine_must_match_descriptor() {
    TemporaryDirectory directory;
    const auto model_path = directory.path() / "model.json";
    write_file(model_path, "{"
                           "\"format\":\"kadoka.model.v1\","
                           "\"model_id\":\"kadoka.obake_kadoka\","
                           "\"model_kind\":\"character_evaluator_randomizer\","
                           "\"engine\":\"builtin:unexpected\","
                           "\"assets\":[]"
                           "}");

    bool failed = false;
    try {
        static_cast<void>(load_obake_kadoka_model(model_path.string()));
    } catch (const std::runtime_error&) {
        failed = true;
    }
    KADOKA_REQUIRE(failed);
}

void test_engines_reject_unrecognized_required_assets() {
    TemporaryDirectory directory;
    write_file(directory.path() / "evaluator.json", "{}");
    write_file(directory.path() / "behavior.json", "{}");
    const auto model_path = directory.path() / "model.json";
    write_file(model_path, "{"
                           "\"format\":\"kadoka.model.v1\","
                           "\"model_id\":\"kadoka.obake_kadoka\","
                           "\"model_kind\":\"character_evaluator_randomizer\","
                           "\"engine\":\"builtin:kadoka.obake_kadoka\","
                           "\"assets\":["
                           "{\"id\":\"evaluator\",\"type\":\"kadoka.evaluator.linear.v1\","
                           "\"path\":\"evaluator.json\",\"required\":true},"
                           "{\"id\":\"behavior\",\"type\":\"kadoka.character_behavior.v1\","
                           "\"path\":\"behavior.json\",\"required\":true},"
                           "{\"id\":\"extra\",\"type\":\"kadoka.script_evaluator.v1\","
                           "\"path\":\"evaluator.json\",\"required\":true}"
                           "]"
                           "}");
    bool obake_failed = false;
    try {
        static_cast<void>(load_obake_kadoka_model(model_path.string()));
    } catch (const std::runtime_error&) {
        obake_failed = true;
    }
    KADOKA_REQUIRE(obake_failed);

    ModelRootDescriptor evaluator_model;
    evaluator_model.engine = "kadoka.evaluator_ai.v1";
    evaluator_model.assets = {
        {"evaluator", "kadoka.script_evaluator.v1", "evaluator.json", true},
        {"extra", "kadoka.evaluator.linear.v1", "evaluator.json", true},
    };
    bool evaluator_failed = false;
    try {
        static_cast<void>(EvaluatorAI("test.evaluator", evaluator_model));
    } catch (const std::runtime_error&) {
        evaluator_failed = true;
    }
    KADOKA_REQUIRE(evaluator_failed);
}

void test_asset_path_must_stay_inside_package() {
    TemporaryDirectory directory;
    const auto model_path = directory.path() / "model.json";
    write_file(model_path, descriptor_with_assets("[{\"id\":\"future\",\"type\":\"kadoka.future_asset.v2\","
                                                  "\"path\":\"../outside.bin\",\"required\":false}]"));
    expect_load_failure(model_path);
}

} // namespace

int main() {
    test_valid_descriptor_and_optional_unknown_asset();
    test_required_asset_must_exist();
    test_unknown_required_asset_type_fails();
    test_duplicate_asset_id_fails();
    test_invalid_format_and_json_fail();
    test_non_json_whitespace_fails();
    test_control_z_does_not_hide_trailing_data();
    test_invalid_utf8_fails();
    test_asset_required_field_is_mandatory();
    test_obake_engine_rejects_mismatched_asset_type();
    test_obake_engine_must_match_descriptor();
    test_engines_reject_unrecognized_required_assets();
    test_asset_path_must_stay_inside_package();
    return 0;
}
