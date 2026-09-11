#pragma once

#include <string>
#include <vector>

namespace kadoka::othello {

enum class DataFieldType {
    String,
    Integer,
    Float,
    Boolean,
    Board,
    Position,
    PositionList,
    CandidateList,
    DiagnosticMap,
};

enum class ConversionRuleKind {
    Direct,
    Rename,
    EncodeBoardFlat,
    EncodeBoardString,
    DecodeBoardFlat,
    EncodePositionPair,
    EncodePositionIndex,
    DecodePositionPair,
    EncodePositionList,
    DecodePositionList,
    EncodeJson,
    DecodeJson,
    Constant,
    Ignore,
};

struct FieldMapping {
    std::string kadoka_field;
    std::string external_field;
    DataFieldType field_type{DataFieldType::String};
    ConversionRuleKind rule{ConversionRuleKind::Direct};
    std::string parameter;
    bool required{false};
};

struct StaticFormatMapping {
    std::string format_id;
    std::string display_name;
    std::string default_extension;
    bool supports_import{true};
    bool supports_export{true};
    std::vector<FieldMapping> fields;
};

[[nodiscard]] const std::vector<StaticFormatMapping>& builtin_format_mappings();
[[nodiscard]] const StaticFormatMapping* find_format_mapping(const std::string& format_id);

}  // namespace kadoka::othello
