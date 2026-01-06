#ifndef CORAL_UTILITIES_H
#define CORAL_UTILITIES_H

#include <nlohmann/json.hpp> // JSON library

#include <iostream>
#include <map>
#include <string>

#include "coral.h"

using json = nlohmann::json;

namespace coral
{
  /**
   * Validates and fixes hashes in a network JSON by comparing them with the
   * NodeObject registry. For each node in the network, it checks if the hash is
   * present in NodeObject::get_registry(). If not, it searches the registry for
   * a matching type name and updates the hash.
   *
   * @param network_json The JSON representing the network (as returned by
   * Network::to_json())
   * @return The fixed network JSON
   */
  inline auto
  fix_hashes(const json &network_json) -> json
  {
    // Create a copy of the input JSON to modify
    json fixed_json = network_json;

    // Get the registry from NodeObject
    json registry = NodeObject::get_registry();

    // Ensure the workflow structure exists
    if (!fixed_json.contains("workflow") ||
        !fixed_json["workflow"].contains("nodes"))
      {
        std::cerr
          << "Invalid network JSON format: missing workflow.nodes structure\n";
        return fixed_json; // Return original if format is invalid
      }

    // Helper function to validate a type identifier against the registry
    auto validate_type = [&registry](json &obj, const std::string &context) {
      if (!obj.contains("type"))
        return;

      std::string current_type = obj["type"];

      if (!registry.contains(current_type))
        {
          std::cerr << "Warning: " << context << " has unknown type id '"
                    << current_type << "'\n";
        }
    };

    // Process each node in the network
    for (auto &[node_id, node] : fixed_json["workflow"]["nodes"].items())
      {
        // Validate the node's own type id
        validate_type(node, "node " + node_id);

        // Validate hashes in the arguments field if it exists
        if (node.contains("arguments") && node["arguments"].is_array())
          {
            for (size_t i = 0; i < node["arguments"].size(); ++i)
              {
                json &arg = node["arguments"][i];
                validate_type(arg,
                              "argument " + std::to_string(i) + " of node " + node_id);
              }
          }

        if (node.contains("base") && node.contains("type"))
          {
            std::string current_base_hash = node["base"];
            std::string current_type_hash = node["type"];

            if (!registry.contains(current_type_hash))
              {
                std::cerr << "Base check: unknown type id " << current_type_hash
                          << " while validating node " << node_id << "\n";
                continue;
              }

            if (!registry[current_type_hash].contains("base"))
              {
                std::cerr << "Base check: type " << current_type_hash
                          << " has no base in registry but node provides one\n";
                continue;
              }

            std::string fixed_base_hash = registry[current_type_hash]["base"];
            if (fixed_base_hash != current_base_hash)
              {
                std::cout << "Fixing type " << current_type_hash
                          << " base hash " << current_base_hash << " -> "
                          << fixed_base_hash << "\n";
                node["base"] = fixed_base_hash;
              }
          }

      }

    return fixed_json;
  }
} // namespace coral
#endif
