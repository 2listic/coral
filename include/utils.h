#ifndef UTILS_H
#define UTILS_H

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace utils
{

    inline auto
    is_json_subset_of(const json& j1, const json& j2) -> bool
    {
        if (j1.is_null()) {
            return true;
        }

        if (j1.type() != j2.type()) {
            return false;
        }

        switch (j1.type()) {
            case json::value_t::object: {
                for (auto& [key, value] : j1.items()) {
                    if (!j2.contains(key) || !is_json_subset_of(value, j2[key])) {
                        return false;
                    }
                }
                return true;
            }

            case json::value_t::array: {
                // For arrays, check if every element in j1 exists somewhere in j2
                for (const auto& elem1 : j1) {
                    bool found = false;
                    for (const auto& elem2 : j2) {
                        if (is_json_subset_of(elem1, elem2)) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        return false;
                    }
                }
                return true;
            }

            default:
                return j1 == j2;
        }
    }
} // namespace utils

#endif // UTILS_H
