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

    // Create a map of type names to their correct hashes from the registry
    std::map<std::string, std::string> type_to_hash;
    for (const auto &[hash, info] : registry.items())
      {
        if (info.contains("type"))
          {
            std::string type_name   = info["type"];
            type_to_hash[type_name] = hash;
          }
      }

    // Ensure the workflow structure exists
    if (!fixed_json.contains("workflow") ||
        !fixed_json["workflow"].contains("nodes"))
      {
        std::cerr
          << "Invalid network JSON format: missing workflow.nodes structure\n";
        return fixed_json; // Return original if format is invalid
      }

    // Process each node in the network
    for (auto &[node_id, node] : fixed_json["workflow"]["nodes"].items())
      {
        // Skip if the node doesn't have a type_hash or type
        if (!node.contains("type_hash") || !node.contains("type"))
          continue;

        std::string current_hash = node["type_hash"];
        std::string type_name    = node["type"];

        // Check if the current hash exists in the registry
        if (!registry.contains(current_hash))
          {
            // Hash not found in registry, try to find by type name
            if (type_to_hash.find(type_name) != type_to_hash.end())
              {
                // Found a match by type name, update the hash
                std::string correct_hash = type_to_hash[type_name];
                std::cout << "Fixing hash for node " << node_id << " of type '"
                          << type_name << "': " << current_hash << " -> "
                          << correct_hash << "\n";

                node["type_hash"] = correct_hash;
              }
            else
              {
                std::cerr
                  << "Warning: Node " << node_id << " of type '" << type_name
                  << "' has invalid hash and no matching type found in registry\n";
              }
          }
      }

    return fixed_json;
  }
} // namespace coral
#endif
