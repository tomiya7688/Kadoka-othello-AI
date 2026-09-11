#include "kadoka_othello/conversion.hpp"

namespace kadoka::othello {
namespace {

StaticFormatMapping make_csv_mapping() {
    return StaticFormatMapping{
        "csv.v1",
        "CSV",
        ".csv",
        true,
        true,
        {
            {"model_id", "model_id", DataFieldType::String, ConversionRuleKind::Direct, "", false},
            {"game_id", "game_id", DataFieldType::String, ConversionRuleKind::Direct, "", false},
            {"ply", "ply", DataFieldType::Integer, ConversionRuleKind::Direct, "", true},
            {"board_size", "board_size", DataFieldType::Integer, ConversionRuleKind::Direct, "", true},
            {"player", "player", DataFieldType::String, ConversionRuleKind::Rename, "black=1;white=2", true},
            {"board", "board", DataFieldType::Board, ConversionRuleKind::EncodeBoardString, "empty=0;black=1;white=2", true},
            {"legal_moves", "legal_moves", DataFieldType::PositionList, ConversionRuleKind::EncodeJson, "", false},
            {"selected_move", "selected_move", DataFieldType::Position, ConversionRuleKind::EncodePositionPair, "row,col", true},
            {"candidates", "candidates", DataFieldType::CandidateList, ConversionRuleKind::EncodeJson, "", false},
            {"diagnostics", "diagnostics", DataFieldType::DiagnosticMap, ConversionRuleKind::EncodeJson, "", false},
        }};
}

StaticFormatMapping make_tsv_mapping() {
    auto mapping = make_csv_mapping();
    mapping.format_id = "tsv.v1";
    mapping.display_name = "TSV";
    mapping.default_extension = ".tsv";
    return mapping;
}

StaticFormatMapping make_json_mapping() {
    return StaticFormatMapping{
        "json.v1",
        "JSON",
        ".json",
        true,
        true,
        {
            {"format_version", "format_version", DataFieldType::String, ConversionRuleKind::Direct, "", false},
            {"model_id", "model_id", DataFieldType::String, ConversionRuleKind::Direct, "", false},
            {"game_id", "game_id", DataFieldType::String, ConversionRuleKind::Direct, "", false},
            {"ply", "ply", DataFieldType::Integer, ConversionRuleKind::Direct, "", true},
            {"board_size", "board_size", DataFieldType::Integer, ConversionRuleKind::Direct, "", true},
            {"player", "player", DataFieldType::String, ConversionRuleKind::Direct, "", true},
            {"board", "board", DataFieldType::Board, ConversionRuleKind::EncodeBoardFlat, "empty=0;black=1;white=2", true},
            {"legal_moves", "legal_moves", DataFieldType::PositionList, ConversionRuleKind::Direct, "", false},
            {"selected_move", "selected_move", DataFieldType::Position, ConversionRuleKind::Direct, "", true},
            {"candidates", "candidates", DataFieldType::CandidateList, ConversionRuleKind::Direct, "", false},
            {"diagnostics", "diagnostics", DataFieldType::DiagnosticMap, ConversionRuleKind::Direct, "", false},
        }};
}

StaticFormatMapping make_jsonl_mapping() {
    auto mapping = make_json_mapping();
    mapping.format_id = "jsonl.v1";
    mapping.display_name = "JSON Lines";
    mapping.default_extension = ".jsonl";
    return mapping;
}

StaticFormatMapping make_numpy_npz_mapping() {
    return StaticFormatMapping{
        "numpy.npz.v1",
        "NumPy NPZ",
        ".npz",
        true,
        true,
        {
            {"board", "boards", DataFieldType::Board, ConversionRuleKind::EncodeBoardFlat, "dtype=int8;empty=0;black=1;white=2", true},
            {"board_size", "board_sizes", DataFieldType::Integer, ConversionRuleKind::Direct, "dtype=int16", true},
            {"player", "players", DataFieldType::Integer, ConversionRuleKind::Rename, "black=1;white=2", true},
            {"selected_move", "selected_moves", DataFieldType::Position, ConversionRuleKind::EncodePositionIndex, "index=row*board_size+col", true},
            {"legal_moves", "legal_moves", DataFieldType::PositionList, ConversionRuleKind::EncodePositionList, "index=row*board_size+col;padding=-1", false},
            {"candidates", "candidate_values", DataFieldType::CandidateList, ConversionRuleKind::EncodeJson, "tool-side structured array", false},
            {"diagnostics", "diagnostics", DataFieldType::DiagnosticMap, ConversionRuleKind::EncodeJson, "utf8-json", false},
        }};
}

StaticFormatMapping make_pytorch_mapping() {
    auto mapping = make_numpy_npz_mapping();
    mapping.format_id = "pytorch.tensor.v1";
    mapping.display_name = "PyTorch Tensor Dataset";
    mapping.default_extension = ".pt";
    mapping.fields[0].parameter = "dtype=int8;tensor=Nx(board_size*board_size)";
    return mapping;
}

StaticFormatMapping make_plain_kifu_mapping() {
    return StaticFormatMapping{
        "othello.kifu.text.v1",
        "Plain Othello Move List",
        ".txt",
        true,
        true,
        {
            {"board_size", "board_size", DataFieldType::Integer, ConversionRuleKind::Constant, "default=8;optional-header", false},
            {"selected_move", "moves", DataFieldType::Position, ConversionRuleKind::EncodePositionPair, "A1-style or row,col;sequence", true},
            {"board", "", DataFieldType::Board, ConversionRuleKind::Ignore, "reconstruct from initial board and moves", false},
            {"legal_moves", "", DataFieldType::PositionList, ConversionRuleKind::Ignore, "recompute from board", false},
            {"candidates", "", DataFieldType::CandidateList, ConversionRuleKind::Ignore, "not representable", false},
            {"diagnostics", "", DataFieldType::DiagnosticMap, ConversionRuleKind::Ignore, "not representable", false},
        }};
}

}  // namespace

const std::vector<StaticFormatMapping>& builtin_format_mappings() {
    static const std::vector<StaticFormatMapping> mappings{
        make_csv_mapping(),
        make_tsv_mapping(),
        make_json_mapping(),
        make_jsonl_mapping(),
        make_numpy_npz_mapping(),
        make_pytorch_mapping(),
        make_plain_kifu_mapping(),
    };
    return mappings;
}

const StaticFormatMapping* find_format_mapping(const std::string& format_id) {
    for (const auto& mapping : builtin_format_mappings()) {
        if (mapping.format_id == format_id) {
            return &mapping;
        }
    }
    return nullptr;
}

}  // namespace kadoka::othello
