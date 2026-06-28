#include "../include/archive/TypeCompatibility.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
struct RecoveredClassLineage {
    std::uint32_t class_id;
    std::uint32_t parent_class_id;
    std::uint32_t module_id;
    std::string_view name;
};

struct RecoveredTypeIdentity {
    std::uint32_t class_id;
    std::uint32_t module_id;
    std::string_view name;
};

[[nodiscard]] constexpr auto recovered_class_lineage() {
    // Recovered from FTMLCore/FTMLSysXMSE static ClassData blocks and ClassInfo::derivesFrom chain.
    return std::array{
        RecoveredClassLineage{0x001D4E16u, 0u,          0x001D509Du, "FTML::Countable"},
        RecoveredClassLineage{0x001D3AC1u, 0x001D4E16u, 0x001D509Du, "FTML::Archive"},
        RecoveredClassLineage{0x04D69F50u, 0x001D3AC1u, 0x001D509Du, "FTML::BinaryArchive"},
        RecoveredClassLineage{0x002C7709u, 0x001D3AC1u, 0x001D509Du, "FTML::TextArchive"},
        RecoveredClassLineage{0x001D3B7Fu, 0x00000000u, 0x001D509Du, "FTML::Array<>"},
        RecoveredClassLineage{0x001D3FBEu, 0x001D4E16u, 0x001D509Du, "FTML::Array<char>"},
        RecoveredClassLineage{0x001D3B11u, 0x001D4E16u, 0x001D509Du, "FTML::Array<TaggedObjectList::Item>"},
        RecoveredClassLineage{0x001D4C1Au, 0x001D4E16u, 0x001D509Du, "FTML::Printf"},
        RecoveredClassLineage{0x001D445Eu, 0x001D4C1Au, 0x001D509Du, "FTML::Debug"},
        RecoveredClassLineage{0x001D4F73u, 0x001D4C1Au, 0x001D509Du, "FTML::String"},
        RecoveredClassLineage{0x001D4F94u, 0x001D4C1Au, 0x001D509Du, "FTML::WString"},
        RecoveredClassLineage{0x127B0867u, 0x001D4C1Au, 0x001D509Du, "FTML::PrintFilter"},
        RecoveredClassLineage{0x001D60B9u, 0x001D4E16u, 0x001D509Du, "FTML::Scanner"},
        RecoveredClassLineage{0x00C5031Bu, 0x001D4E16u, 0x001D509Du, "FTML::ScanQueue"},
        RecoveredClassLineage{0x001D3AE9u, 0x001D4E16u, 0x001D509Du, "FTML::TaggedObjectList"},
        // FTMLSysXMSE reverse-verified from constructor base calls and static ClassData blocks.
        RecoveredClassLineage{0xDCF62C00u, 0x0197F267u, 0xDCCC9F00u, "FTML::XMSE::DCRecipe"},
        RecoveredClassLineage{0xDCF65100u, 0x0198277Bu, 0xDCCC9F00u, "FTML::XMSE::DPRecipe"},
        RecoveredClassLineage{0xDE284800u, 0x001D4E16u, 0xDCCC9F00u, "FTML::XMSE::RawData"},
        RecoveredClassLineage{0xDE281900u, 0x01A2C46Au, 0xDCCC9F00u, "FTML::XMSE::RawDataSet"},
        RecoveredClassLineage{0xDE36D000u, 0x018514A1u, 0xDCCC9F00u, "FTML::XMSE::SubSystem"},
    };
}

[[nodiscard]] const std::unordered_map<std::uint32_t, RecoveredClassLineage>& recovered_lineage_by_id() {
    static const auto value = [] {
        std::unordered_map<std::uint32_t, RecoveredClassLineage> map;
        for(const auto& item : recovered_class_lineage()){
            map.emplace(item.class_id, item);
        }
        return map;
    }();
    return value;
}

[[nodiscard]] const RecoveredClassLineage* find_recovered_lineage(std::uint32_t class_id) {
    const auto& map = recovered_lineage_by_id();
    const auto it = map.find(class_id);
    if(it == map.end()){
        return nullptr;
    }
    return &it->second;
}

[[nodiscard]] constexpr auto recovered_type_identities() {
    return std::array{
        RecoveredTypeIdentity{0x001D4E16u, 0x001D509Du, "FTML::Countable"},
        RecoveredTypeIdentity{0x001D3AC1u, 0x001D509Du, "FTML::Archive"},
        RecoveredTypeIdentity{0x04D69F50u, 0x001D509Du, "FTML::BinaryArchive"},
        RecoveredTypeIdentity{0x002C7709u, 0x001D509Du, "FTML::TextArchive"},
        RecoveredTypeIdentity{0x491A1E3Cu, 0xDCCC9F00u, "FTML::DC::Configuration"},
        RecoveredTypeIdentity{0x0197F267u, 0xDCCC9F00u, "FTML::DC::DCRecipe"},
        RecoveredTypeIdentity{0x0198277Bu, 0xDCCC9F00u, "FTML::DC::DPRecipe"},
        RecoveredTypeIdentity{0x01A2C46Au, 0xDCCC9F00u, "FTML::DC::RawDataSet"},
        RecoveredTypeIdentity{0x018514A1u, 0xDCCC9F00u, "FTML::DC::SubSystem"},
        RecoveredTypeIdentity{0xDCF62C00u, 0xDCCC9F00u, "FTML::XMSE::DCRecipe"},
        RecoveredTypeIdentity{0xDCF65100u, 0xDCCC9F00u, "FTML::XMSE::DPRecipe"},
        RecoveredTypeIdentity{0xDE284800u, 0xDCCC9F00u, "FTML::XMSE::RawData"},
        RecoveredTypeIdentity{0xDE281900u, 0xDCCC9F00u, "FTML::XMSE::RawDataSet"},
        RecoveredTypeIdentity{0xDE36D000u, 0xDCCC9F00u, "FTML::XMSE::SubSystem"},
    };
}

[[nodiscard]] const RecoveredTypeIdentity* find_recovered_type_identity(std::uint32_t class_id) {
    static const auto identities = [] {
        std::unordered_map<std::uint32_t, RecoveredTypeIdentity> map;
        for(const auto& item : recovered_type_identities()){
            map.emplace(item.class_id, item);
        }
        return map;
    }();

    const auto it = identities.find(class_id);
    if(it == identities.end()){
        return nullptr;
    }
    return &it->second;
}

[[nodiscard]] std::string_view recovered_module_name(std::uint32_t module_id) {
    switch(module_id){
    case 0x001D509Du:
        return "FTMLCore";
    case 0xDCCC9F00u:
        return "FTMLSysXMSE";
    default:
        return {};
    }
}

void add_edge(std::unordered_map<std::string, std::vector<std::string>>& graph, std::string child, std::string parent) {
    graph[std::move(child)].push_back(std::move(parent));
}

[[nodiscard]] const std::unordered_map<std::string, std::vector<std::string>>& inheritance_graph() {
    static const auto graph = [] {
        std::unordered_map<std::string, std::vector<std::string>> value;
        add_edge(value, "FTML::Archive", "FTML::Countable");
        add_edge(value, "FTML::BinaryArchive", "FTML::Archive");
        add_edge(value, "FTML::TextArchive", "FTML::Archive");
        return value;
    }();
    return graph;
}

[[nodiscard]] std::vector<std::string> candidate_names(const OLSA::Container::ResolvedTypeInfo& type) {
    std::vector<std::string> names;
    if(!type.class_name.empty()){
        names.push_back(type.class_name);
    }
    if(const auto* recovered = find_recovered_lineage(type.class_id); recovered != nullptr){
        const std::string recovered_name{recovered->name};
        if(std::find(names.begin(), names.end(), recovered_name) == names.end()){
            names.push_back(recovered_name);
        }
    }
    const auto display = type.display_name();
    if(!display.empty() && std::find(names.begin(), names.end(), display) == names.end()){
        names.push_back(display);
    }
    return names;
}

[[nodiscard]] bool any_name_matches(const std::vector<std::string>& lhs, const std::vector<std::string>& rhs) {
    for(const auto& left : lhs){
        for(const auto& right : rhs){
            if(left == right){
                return true;
            }
        }
    }
    return false;
}

[[nodiscard]] bool all_known_types_derive_from_countable(
    const OLSA::Container::ResolvedTypeInfo& actual,
    const std::vector<std::string>& expected_names) {
    if(!actual.known){
        return false;
    }

    static constexpr std::array<std::string_view, 2> countable_names = {
        "FTML::Countable",
        "FTMLBase::FTML::Countable",
    };

    for(const auto& expected_name : expected_names){
        if(std::find(countable_names.begin(), countable_names.end(), expected_name) != countable_names.end()){
            return true;
        }
    }
    return false;
}

[[nodiscard]] bool derives_from_graph(
    const std::vector<std::string>& actual_names,
    const std::vector<std::string>& expected_names) {
    const auto& graph = inheritance_graph();
    std::unordered_set<std::string> visited;
    std::vector<std::string> worklist = actual_names;

    while(!worklist.empty()){
        auto current = std::move(worklist.back());
        worklist.pop_back();
        if(!visited.insert(current).second){
            continue;
        }

        for(const auto& expected_name : expected_names){
            if(current == expected_name){
                return true;
            }
        }

        const auto it = graph.find(current);
        if(it == graph.end()){
            continue;
        }
        for(const auto& parent : it->second){
            worklist.push_back(parent);
        }
    }
    return false;
}

[[nodiscard]] bool derives_from_recovered_lineage(std::uint32_t actual_class_id, std::uint32_t expected_class_id) {
    if(actual_class_id == 0 || expected_class_id == 0){
        return false;
    }

    std::unordered_set<std::uint32_t> visited;
    auto current = actual_class_id;
    while(current != 0 && visited.insert(current).second){
        if(current == expected_class_id){
            return true;
        }

        const auto* current_info = find_recovered_lineage(current);
        if(current_info == nullptr){
            return false;
        }
        current = current_info->parent_class_id;
    }
    return false;
}
}  // namespace

FTML::SmartPointer::Compatibility OLSA::ArchiveUtil::classify_type_compatibility(
    const OLSA::Container::ResolvedTypeInfo& actual,
    const OLSA::Container::ResolvedTypeInfo& expected) {
    using FTML::SmartPointer::Compatibility;

    if(expected.class_id == 0){
        return Compatibility::NoExpectation;
    }
    if(actual.class_id == 0){
        return Compatibility::Unresolved;
    }
    if(actual.class_id == expected.class_id){
        return Compatibility::Exact;
    }
    if(derives_from_recovered_lineage(actual.class_id, expected.class_id)){
        return Compatibility::Derived;
    }
    if(!expected.known && find_recovered_lineage(expected.class_id) == nullptr){
        return Compatibility::UnknownExpected;
    }

    const auto actual_names = candidate_names(actual);
    const auto expected_names = candidate_names(expected);
    if(actual_names.empty() || expected_names.empty()){
        return Compatibility::Different;
    }
    if(any_name_matches(actual_names, expected_names)){
        return Compatibility::Exact;
    }
    if(all_known_types_derive_from_countable(actual, expected_names)){
        return Compatibility::Derived;
    }
    if(derives_from_graph(actual_names, expected_names)){
        return Compatibility::Derived;
    }
    return Compatibility::Different;
}

std::optional<OLSA::Container::ResolvedTypeInfo> OLSA::ArchiveUtil::resolve_recovered_type_info(std::uint32_t class_id) {
    const auto* recovered = find_recovered_type_identity(class_id);
    if(recovered == nullptr){
        return std::nullopt;
    }

    OLSA::Container::ResolvedTypeInfo info{};
    info.class_id = recovered->class_id;
    info.module_id = recovered->module_id;
    info.class_name = std::string(recovered->name);
    info.module_name = std::string(recovered_module_name(recovered->module_id));
    info.known = true;
    return info;
}
