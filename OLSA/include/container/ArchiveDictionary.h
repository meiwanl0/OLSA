#pragma once

#include <cstdint>
#include <string_view>
#include <string>

namespace OLSA::Container {
struct ClassItem {
    std::uint32_t class_id{};
    std::string name;
    std::uint16_t version{};
    std::uint32_t module_id{};
};

struct ModuleItem {
    std::uint32_t module_id{};
    std::string name;
    std::string version;
};

struct TaggedObject {
    std::uint64_t tag{};
    std::uint32_t class_id{};
};

struct ResolvedTypeInfo {
    std::uint32_t class_id{};
    std::uint16_t class_version{};
    std::uint32_t module_id{};
    std::string class_name;
    std::string module_name;
    std::string module_version;
    bool is_pointer{};
    bool from_tag{};
    bool known{};

    [[nodiscard]] std::string display_name() const {
        if(module_name.empty()){
            return class_name;
        }
        if(class_name.empty()){
            return module_name;
        }
        if(class_name.rfind(module_name + "::", 0) == 0){
            return class_name;
        }
        return module_name + "::" + class_name;
    }
};
}  // namespace OLSA::Container
