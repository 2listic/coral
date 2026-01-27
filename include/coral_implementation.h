#ifndef CORAL_IMPLEMENTATION_H
#define CORAL_IMPLEMENTATION_H

#include "coral.h"

#if defined(CORAL_HEADER_ONLY) && CORAL_HEADER_ONLY
#  define CORAL_IMPL_INLINE inline
#else
#  define CORAL_IMPL_INLINE
#endif

namespace coral
{
  CORAL_IMPL_INLINE
  NodeObject::NodeObject(const std::shared_ptr<entt::meta_any> &data)
    : NodeObject(detail::hash(data))
  {}

  CORAL_IMPL_INLINE
  NodeObject::NodeObject(const std::string &hash_str)
  {
    try
      {
        initializer = initializers.at(hash_str);
      }
    catch (const std::out_of_range &)
      {
        // If we did not find the hash, this is actually an std::string
        // object-.
        *this = NodeObject(std::make_shared<std::string>(hash_str));
      }
    initialize_arguments();
    initialize_inputs();
    initialize_outputs();
    setup_network_if_needed();
  }

  CORAL_IMPL_INLINE
  NodeObject::NodeObject(const char *hash_str)
    : NodeObject(std::string(hash_str))
  {}

  CORAL_IMPL_INLINE auto
  NodeObject::get_registry() -> json
  {
    json registry;
    for (const auto &[hash_str, initializer] : NodeObject::initializers)
      {
        registry[hash_str] = initializer.json_serializer;
      }
    return registry;
  }



  CORAL_IMPL_INLINE auto
  NodeObject::is_network_type(const std::string &hash_str) -> bool
  {
    return !network_type_hash.empty() && hash_str == network_type_hash;
  }



  CORAL_IMPL_INLINE auto
  NodeObject::build_network_interface(
    const std::shared_ptr<entt::meta_any> &value) -> detail::NodeInterface
  {
    if (!network_interface_builder)
      return {};
    return network_interface_builder(value);
  }



  CORAL_IMPL_INLINE void
  NodeObject::override_interface(const json &arguments,
                                 const json &inputs,
                                 const json &outputs)
  {
    initializer.json_serializer["arguments"] = arguments;
    initializer.json_serializer["inputs"]    = inputs;
    initializer.json_serializer["outputs"]   = outputs;
    initialize_arguments();
    initialize_inputs();
    initialize_outputs();
  }



  CORAL_IMPL_INLINE auto
  NodeObject::ready() const -> bool
  {
    return object && (*object);
  }



  CORAL_IMPL_INLINE void
  NodeObject::parse_string(const std::string &value_str)
  {
    if (initializer.parse_string)
      object = initializer.parse_string(value_str);
    else
      {
        throw std::runtime_error(
          "No parse_string function is available for this type.");
      }
  }



  CORAL_IMPL_INLINE auto
  NodeObject::to_string() const -> std::string
  {
    if (initializer.to_string)
      {
        if (!ready())
          throw std::runtime_error("Object is not ready for conversion to "
                                   "string. Please call operator() first.");
        return initializer.to_string(object);
      }



    throw std::runtime_error("Unsupported type for JSON conversion");
  }



  CORAL_IMPL_INLINE auto
  NodeObject::operator()() -> bool
  {
    std::vector<size_t> not_ready_arguments;
    for (size_t i = 0; i < arguments.size(); ++i)
      {
        if ((arguments_types[i] & ConnectionType::input) ==
            ConnectionType::none)
          continue;
        if (!arguments[i])
          {
            not_ready_arguments.push_back(i);
            continue;
          }
        if (!arguments[i]->ready())
          {
            not_ready_arguments.push_back(i);
          }
      }
    if (!not_ready_arguments.empty())
      {
        std::string error_msg =
          "Arguments are not ready or connected. Not ready argument indices: ";
        for (size_t i = 0; i < not_ready_arguments.size(); ++i)
          {
            error_msg += std::to_string(not_ready_arguments[i]);
            if (i + 1 < not_ready_arguments.size())
              error_msg += ", ";
          }
        throw std::runtime_error(error_msg);
      }

    if (initializer.node_type == NodeType::elementary_constructor &&
        initializer.to_string && object && *object)
      initializer.json_serializer["value"] = initializer.to_string(object);

    // Execution is only safe when the node is owned by a shared_ptr.
    NodeObjectPtr self;
    try
      {
        self = shared_from_this();
      }
    catch (const std::bad_weak_ptr &)
      {
        throw std::runtime_error(
          "NodeObject execution requires shared ownership.");
      }
    object = initializer.executor(self, arguments);
    // Check if we have to copy back the original value
    if (initializer.json_serializer.contains("value") &&
        initializer.parse_string)
      object = initializer.parse_string(
        initializer.json_serializer["value"].template get<std::string>());
    return object.operator bool();
  }



  CORAL_IMPL_INLINE void
  NodeObject::set_arguments(const std::vector<NodeObjectPtr> &args)
  {
    if (args.size() != initializer.json_serializer["arguments"].size())
      throw std::runtime_error(
        "Wrong number of arguments: " + std::to_string(args.size()) +
        " instead of " +
        std::to_string(initializer.json_serializer["arguments"].size()) + ".");
    if (arguments_types.size() !=
        initializer.json_serializer["arguments"].size())
      initialize_arguments();
    this->arguments = args;
  }



  CORAL_IMPL_INLINE void
  NodeObject::initialize_inputs()
  {
    const auto &json_inputs = initializer.json_serializer["inputs"];
    input_indices.resize(json_inputs.size());
    input_bound.assign(json_inputs.size(), false);
    for (unsigned int i = 0; i < json_inputs.size(); ++i)
      {
        input_indices[i] = json_inputs[i].get<unsigned int>();
        input_bound[i]   = (input_indices[i] == -1);
      }
  }



  CORAL_IMPL_INLINE void
  NodeObject::initialize_outputs()
  {
    const auto &json_outputs = initializer.json_serializer["outputs"];
    output_indices.resize(json_outputs.size());
    output_bound.assign(json_outputs.size(), false);
    for (unsigned int i = 0; i < json_outputs.size(); ++i)
      {
        output_indices[i] = json_outputs[i].get<int>();
        if (output_indices[i] != -1)
          {
            // Throw and exception if the output index is not valid.
            if (output_indices[i] >=
                static_cast<int>(
                  initializer.json_serializer["arguments"].size()))
              throw std::runtime_error(
                "Output index " + std::to_string(output_indices[i]) +
                " is out of bounds for the number of arguments: " +
                std::to_string(
                  initializer.json_serializer["arguments"].size()) +
                ".");
            arguments[output_indices[i]] = std::make_shared<NodeObject>(
              initializer
                .json_serializer["arguments"][output_indices[i]]["type"]
                .get<std::string>());
          }
        else
          {
            output_bound[i] = true;
          }
      }
  }



  CORAL_IMPL_INLINE void
  NodeObject::initialize_arguments()
  {
    const auto &json_args = initializer.json_serializer["arguments"];
    arguments.resize(json_args.size());
    arguments_types.resize(json_args.size(), ConnectionType::none);
    for (unsigned int i = 0; i < json_args.size(); ++i)
      {
        if (json_args[i].contains("connection_type"))
          arguments_types[i] =
            magic_enum::enum_cast<ConnectionType>(
              json_args[i].at("connection_type").get<std::string>())
              .value();
      }
  }



  CORAL_IMPL_INLINE void
  NodeObject::setup_network_if_needed()
  {
    const auto &type = initializer.json_serializer.at("type");
    if (!NodeObject::is_network_type(type.get<std::string>()))
      return;

    if (ready())
      {
        const auto iface = NodeObject::build_network_interface(object);
        if (!iface.arguments.empty() || !iface.inputs.empty() ||
            !iface.outputs.empty())
          override_interface(iface.arguments, iface.inputs, iface.outputs);
      }
  }



  CORAL_IMPL_INLINE void
  NodeObject::bind_inputs(
    const std::vector<std::pair<NodeObjectPtr, unsigned int>> &inputs)
  {
    if (inputs.size() != input_indices.size())
      throw std::runtime_error(
        "Wrong number of inputs: " + std::to_string(inputs.size()) +
        " instead of " + std::to_string(input_indices.size()) + ".");

    for (unsigned int i = 0; i < inputs.size(); ++i)
      {
        const auto &input_node  = inputs[i].first;
        const auto &input_index = inputs[i].second;

        const auto expected_hash =
          initializer.json_serializer["arguments"][input_indices[i]]["type"]
            .get<std::string>();
        const auto input_hash = input_node->get_output(input_index)->hash();

        // Check if the input hash matches the expected hash or is derived
        // from the base type
        const auto &base_hash =
          initializer.json_serializer["arguments"][input_indices[i]].value(
            "base", "");
        const bool is_valid =
          (expected_hash == input_hash) || (base_hash == input_hash);

        if (!is_valid)
          {
            const std::string base_suffix =
              base_hash.empty() ? std::string() :
                                  " or its base type (" + base_hash + ")";
            throw std::runtime_error("The hash type of input " +
                                     std::to_string(i) + " (" + input_hash +
                                     ") does not match the expected hash (" +
                                     expected_hash + ")" + base_suffix + ".");
          }

        arguments[input_indices[i]] = input_node->get_output(input_index);
        input_bound[i]              = true;
      }
  }



  CORAL_IMPL_INLINE NodeObjectPtr
  NodeObject::get_output(const unsigned int index)
  {
    if (!(index < output_indices.size()))
      throw std::runtime_error("Index out of bounds.");
    if (output_indices[index] == -1)
      {
        return shared_from_this(); // Return this object for 'self'
      }
    return arguments[output_indices[index]];
  }



  CORAL_IMPL_INLINE bool
  NodeObject::is_input_bound(const unsigned int index) const
  {
    if (!(index < input_indices.size()))
      throw std::runtime_error(
        "Index out of bounds: you asked for input " + std::to_string(index) +
        ", but there are " + std::to_string(input_indices.size()) +
        " inputs: " + initializer.json_serializer.dump(2));
    if (input_indices[index] == -1)
      return true;
    return input_bound[index];
  }



  CORAL_IMPL_INLINE bool
  NodeObject::is_passthrough_input(const unsigned int index) const
  {
    if (!(index < input_indices.size()))
      throw std::runtime_error(
        "Index out of bounds: you asked for input " + std::to_string(index) +
        ", but there are " + std::to_string(input_indices.size()) +
        " inputs: " + initializer.json_serializer.dump(2));
    if (input_indices[index] == -1)
      return false;
    const auto arg_id = input_indices[index];
    if (!(arg_id >= 0 && arg_id < static_cast<int>(arguments_types.size())))
      throw std::runtime_error("Input argument index out of bounds.");
    return (arguments_types[arg_id] & ConnectionType::pass_through) ==
           ConnectionType::pass_through;
  }



  CORAL_IMPL_INLINE bool
  NodeObject::is_output_bound(const unsigned int index) const
  {
    if (!(index < output_indices.size()))
      throw std::runtime_error("Index out of bounds.");
    if (output_indices[index] == -1)
      return true;
    if (is_passthrough_output(index))
      return is_input_bound(input_index_for_argument(output_indices[index]));
    return output_bound[index];
  }



  CORAL_IMPL_INLINE bool
  NodeObject::is_bindable(const unsigned int index) const
  {
    if (!(index < output_indices.size()))
      throw std::runtime_error("Index out of bounds.");
    return output_indices[index] != -1 && !is_passthrough_output(index);
  }



  CORAL_IMPL_INLINE void
  NodeObject::bind_output(const unsigned int index, const NodeObjectPtr &value)
  {
    if (!(index < output_indices.size()))
      throw std::runtime_error("Index out of bounds.");
    if (output_indices[index] == -1)
      throw std::runtime_error("Cannot set output for 'self'.");
    if (is_passthrough_output(index))
      throw std::runtime_error("Cannot bind pass-through output.");
    if (!value)
      throw std::runtime_error("Cannot bind output to null.");

    arguments[output_indices[index]] = value;
    output_bound[index]              = true;
  }



  CORAL_IMPL_INLINE bool
  NodeObject::has_unbound_inputs() const
  {
    for (unsigned int i = 0; i < input_indices.size(); ++i)
      if (!is_input_bound(i))
        return true;
    return false;
  }



  CORAL_IMPL_INLINE bool
  NodeObject::has_unbound_outputs() const
  {
    for (unsigned int i = 0; i < output_indices.size(); ++i)
      if (!is_output_bound(i))
        return true;
    return false;
  }



  CORAL_IMPL_INLINE NodeObjectPtr
  NodeObject::get_input(const unsigned int index)
  {
    if (!(index < input_indices.size()))
      throw std::runtime_error(
        "Index out of bounds: you asked for input " + std::to_string(index) +
        ", but there are " + std::to_string(input_indices.size()) +
        " inputs: " + initializer.json_serializer.dump(2));
    if (input_indices[index] == -1)
      return shared_from_this(); // Return this object for 'self'
    auto arg_id = input_indices[index];
    if (!(arg_id < static_cast<int>(arguments.size())))
      throw std::runtime_error(
        "Internal error! Input you asked for input " + std::to_string(index) +
        ", which points to argument number " + std::to_string(arg_id) +
        ", but there are only " + std::to_string(arguments.size()) +
        " arguments to pick from.");
    return arguments[input_indices[index]];
  }



  CORAL_IMPL_INLINE void
  NodeObject::bind_input(const unsigned int index, const NodeObjectPtr &value)
  {
    if (!(index < input_indices.size()))
      throw std::runtime_error(
        "Index out of bounds: you asked for input " + std::to_string(index) +
        ", but there are " + std::to_string(input_indices.size()) +
        " inputs: " + initializer.json_serializer.dump(2));
    if (input_indices[index] == -1)
      throw std::runtime_error("Cannot set input for 'self'.");
    if (!value)
      throw std::runtime_error("Cannot bind input to null.");
    auto arg_id = input_indices[index];
    if (!(arg_id < static_cast<int>(arguments.size())))
      throw std::runtime_error(
        "Internal error! You asked to set input " + std::to_string(index) +
        ", which points to argument number " + std::to_string(arg_id) +
        ", but there are only " + std::to_string(arguments.size()) +
        " arguments to pick from.");
    arguments[input_indices[index]] = value;
    input_bound[index]              = true;
  }



  CORAL_IMPL_INLINE const json &
  NodeObject::get_info() const
  {
    auto &j = initializer.json_serializer;
    if (initializer.to_string && object && *object)
      j["value"] = initializer.to_string(object);
    return j;
  }



  CORAL_IMPL_INLINE std::string
                    NodeObject::hash() const
  {
    if (object.operator bool() && (*object))
      {
        // The object is initialized. Check if this is consistent with
        // the initializer, and return its hash.
        const auto object_hash = detail::hash(object);
        const auto stored_hash =
          std::string(initializer.json_serializer.at("type"));
        if (stored_hash.find(object_hash) != 0)
          throw std::runtime_error(
            "Object type does not match: we store " + stored_hash +
            ", and cannot set this object equal to " + object_hash +
            ". The two hashes should at least start with the same characters.");
        return stored_hash;
      }
    return initializer.json_serializer.at("type");
  }



  CORAL_IMPL_INLINE std::string
                    NodeObject::type_name() const
  {
    return initializer.type_name;
  }



  CORAL_IMPL_INLINE NodeType
  NodeObject::node_type() const
  {
    return magic_enum::enum_cast<NodeType>(
             initializer.json_serializer.at("node_type").get<std::string>())
      .value();
  }



  CORAL_IMPL_INLINE size_t
  NodeObject::n_arguments() const
  {
    return initializer.json_serializer["arguments"].size();
  }



  CORAL_IMPL_INLINE size_t
  NodeObject::n_inputs() const
  {
    return initializer.json_serializer["inputs"].size();
  }



  CORAL_IMPL_INLINE size_t
  NodeObject::n_outputs() const
  {
    return initializer.json_serializer["outputs"].size();
  }



  CORAL_IMPL_INLINE bool
  NodeObject::is_passthrough_output(const unsigned int index) const
  {
    if (!(index < output_indices.size()))
      throw std::runtime_error("Index out of bounds.");
    if (output_indices[index] == -1)
      return false;
    const auto arg_id = output_indices[index];
    if (!(arg_id >= 0 && arg_id < static_cast<int>(arguments_types.size())))
      throw std::runtime_error("Output argument index out of bounds.");
    return (arguments_types[arg_id] & ConnectionType::pass_through) ==
           ConnectionType::pass_through;
  }



  CORAL_IMPL_INLINE unsigned int
  NodeObject::input_index_for_argument(const int argument_index) const
  {
    for (unsigned int i = 0; i < input_indices.size(); ++i)
      if (input_indices[i] == argument_index)
        return i;
    throw std::runtime_error("Argument is not exposed as an input.");
  }



  CORAL_IMPL_INLINE void
  NodeObject::set_output_only(detail::NodeObjectInitializer &initializer,
                              unsigned int                   arg_index)
  {
    auto &args = initializer.json_serializer["arguments"];
    if (arg_index >= args.size())
      return;

    args[arg_index]["connection_type"] =
      magic_enum::enum_name(ConnectionType::output);

    auto &inputs = initializer.json_serializer["inputs"];
    for (auto it = inputs.begin(); it != inputs.end();)
      {
        if (it->get<unsigned int>() == arg_index)
          it = inputs.erase(it);
        else
          ++it;
      }



    auto &outputs = initializer.json_serializer["outputs"];
    for (const auto &entry : outputs)
      {
        if (entry.get<int>() == static_cast<int>(arg_index))
          return;
      }
    outputs.push_back(static_cast<int>(arg_index));
  }



  CORAL_IMPL_INLINE void
  to_json(json &j, const NodeObjectPtr &obj)
  {
    j = obj->get_info();
  }



  namespace detail
  {
    CORAL_IMPL_INLINE auto
    validate_and_reorder_network_interface(
      const json                  &args,
      const json                  &inputs,
      const json                  &outputs,
      const detail::NodeInterface &expected_iface)
      -> std::tuple<json, json, json>
    {
      if (!args.is_array() || !inputs.is_array() || !outputs.is_array())
        throw std::runtime_error("Network interface must be arrays.");

      if (args.size() != expected_iface.arguments.size())
        throw std::runtime_error(
          "Network arguments do not match expected size.");

      std::vector<int>  provided_to_expected(args.size(), -1);
      std::vector<bool> expected_matched(expected_iface.arguments.size(),
                                         false);

      for (size_t provided_idx = 0; provided_idx < args.size(); ++provided_idx)
        {
          const auto &provided = args[provided_idx];
          if (!provided.contains("type"))
            throw std::runtime_error("Network argument is missing type.");
          if (!provided.contains("connection_type"))
            throw std::runtime_error(
              "Network argument is missing connection_type.");

          for (size_t expected_idx = 0;
               expected_idx < expected_iface.arguments.size();
               ++expected_idx)
            {
              if (expected_matched[expected_idx])
                continue;

              const auto &expected = expected_iface.arguments[expected_idx];

              if (provided.contains("name") && expected.contains("name") &&
                  provided.at("name") == expected.at("name"))
                {
                  if (provided.at("type") != expected.at("type"))
                    throw std::runtime_error(
                      "Network argument '" +
                      provided.at("name").get<std::string>() +
                      "' type does not match expected value.");
                  if (provided.at("connection_type") !=
                      expected.at("connection_type"))
                    throw std::runtime_error(
                      "Network argument '" +
                      provided.at("name").get<std::string>() +
                      "' connection_type does not match expected value.");
                  provided_to_expected[provided_idx] =
                    static_cast<int>(expected_idx);
                  expected_matched[expected_idx] = true;
                  break;
                }

              if (provided.at("type") == expected.at("type") &&
                  provided.at("connection_type") ==
                    expected.at("connection_type"))
                {
                  provided_to_expected[provided_idx] =
                    static_cast<int>(expected_idx);
                  expected_matched[expected_idx] = true;
                  break;
                }
            }

          if (provided_to_expected[provided_idx] == -1)
            throw std::runtime_error(
              "Network argument does not match any expected argument.");
        }

      std::vector<int> expected_to_provided(expected_iface.arguments.size(),
                                            -1);
      for (size_t provided_idx = 0; provided_idx < args.size(); ++provided_idx)
        expected_to_provided[static_cast<size_t>(
          provided_to_expected[provided_idx])] = static_cast<int>(provided_idx);

      json reordered_args = json::array();
      for (size_t expected_idx = 0; expected_idx < expected_to_provided.size();
           ++expected_idx)
        {
          const int provided_idx = expected_to_provided[expected_idx];
          if (provided_idx < 0)
            throw std::runtime_error(
              "Network argument does not match any expected argument.");
          reordered_args.push_back(args[static_cast<size_t>(provided_idx)]);
        }

      json remapped_inputs = json::array();
      for (const auto &input_idx : inputs)
        {
          const auto provided_idx = input_idx.get<size_t>();
          if (provided_idx >= provided_to_expected.size())
            throw std::runtime_error("Network input index out of range.");
          remapped_inputs.push_back(provided_to_expected[provided_idx]);
        }

      json remapped_outputs = json::array();
      for (const auto &output_idx : outputs)
        {
          const auto provided_idx = output_idx.get<size_t>();
          if (provided_idx >= provided_to_expected.size())
            throw std::runtime_error("Network output index out of range.");
          remapped_outputs.push_back(provided_to_expected[provided_idx]);
        }

      if (remapped_inputs != expected_iface.inputs)
        throw std::runtime_error("Network inputs do not match expected order.");
      if (remapped_outputs != expected_iface.outputs)
        throw std::runtime_error(
          "Network outputs do not match expected order.");

      return std::make_tuple(reordered_args,
                             expected_iface.inputs,
                             expected_iface.outputs);
    }
  } // namespace detail

  CORAL_IMPL_INLINE void
  from_json(const json &j, NodeObjectPtr &obj)
  {
    if (!j.contains("type"))
      throw std::runtime_error(
        "The json does not contain a type entry. Bailing out.");
    obj = make_node(j.at("type").get<std::string>());
    // Make sure the hash matches the expected value
    if (j["type"] != obj->hash())
      throw std::runtime_error(
        "The type does not match the expected value: expected " +
        j["type"].dump() + ", got " + obj->hash());

    const auto &reg        = obj->get_info();
    const bool  is_network = NodeObject::is_network_type(obj->hash());

    // If the input JSON contains fields that also exist in the registry
    // description, they must match exactly (except for "value", which is
    // allowed to differ when provided).
    for (const auto &[key, val] : j.items())
      {
        if (key == "value")
          continue;
        if (is_network &&
            (key == "arguments" || key == "inputs" || key == "outputs"))
          continue;
        if (reg.contains(key) && reg.at(key) != val)
          throw std::runtime_error(
            "The json object field '" + key +
            "' does not match the registered value: expected " +
            reg.at(key).dump() + ", got " + val.dump());
      }

    const auto node_type = reg.value("node_type", "");

    if ((node_type == "elementary_constructor" ||
         node_type == "empty_constructor") &&
        !is_network)
      (*obj)();
    if (j.contains("value"))
      {
        obj->parse_string(j.at("value").get<std::string>());
        if (obj->ready())
          {
            // Refresh serialized value to match the parsed object.
            (void)obj->get_info();
          }
      }

    if (is_network)
      {
        const bool has_any_interface = j.contains("arguments") ||
                                       j.contains("inputs") ||
                                       j.contains("outputs");
        const bool has_full_interface = j.contains("arguments") &&
                                        j.contains("inputs") &&
                                        j.contains("outputs");

        const auto &any      = detail::meta_any_ref(*obj);
        const auto  expected = NodeObject::build_network_interface(any);

        if (has_any_interface)
          {
            if (!has_full_interface)
              throw std::runtime_error(
                "Network interface override requires arguments, inputs, and outputs.");
            auto [reordered_args, reordered_inputs, reordered_outputs] =
              detail::validate_and_reorder_network_interface(j.at("arguments"),
                                                             j.at("inputs"),
                                                             j.at("outputs"),
                                                             expected);
            obj->override_interface(reordered_args,
                                    reordered_inputs,
                                    reordered_outputs);
          }
        else
          {
            obj->override_interface(expected.arguments,
                                    expected.inputs,
                                    expected.outputs);
          }
      }
  }



  CORAL_IMPL_INLINE void
  connect(NodeObjectPtr                                             &node,
          const std::vector<std::pair<NodeObjectPtr, unsigned int>> &inputs)
  {
    node->bind_inputs(inputs);
  }
} // namespace coral

namespace coral::detail
{
  CORAL_IMPL_INLINE std::shared_ptr<entt::meta_any>                   &
  meta_any_ref(NodeObject &node)
  {
    return node.object;
  }



  CORAL_IMPL_INLINE const std::shared_ptr<entt::meta_any>                         &
  meta_any_ref(const NodeObject &node)
  {
    return node.object;
  }
} // namespace coral::detail

#undef CORAL_IMPL_INLINE

#endif
