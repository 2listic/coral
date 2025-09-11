#ifndef CORAL_H
#define CORAL_H

#include <nlohmann/json.hpp> // JSON library

#include <any>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include "magic_enum/magic_enum_all.hpp" // Reintroduced Magic Enum library
#include "type_name.h"                   // Single boost file
#include "utils.h"

/**
 * Refer to README.md for a comprehensive overview of the CORAL library.
 */

using json = nlohmann::json;

namespace coral
{
  using namespace magic_enum::bitwise_operators;

  // forward declarations
  class NodeObject;

  using NodeObjectPtr = std::shared_ptr<NodeObject>;

  template <typename... Args>
  inline std::tuple<
    std::shared_ptr<std::remove_const_t<std::remove_reference_t<Args>>>...>
  cast_args(const std::vector<std::shared_ptr<NodeObject>> &args);

  /**
   * Provide a string that can be used as a hash for a type.
   */
  inline auto
  hash(const std::type_info &type, const std::string &suffix = "")
    -> std::string
  {
    std::stringstream ss;
    ss << std::hex << std::type_index(type).hash_code();
    if (suffix != "")
      ss << suffix;
    return ss.str();
  }

  /**
   * Provide a string that can be used as a hash for a type.
   */
  template <typename T>
  inline auto
  hash(const std::string &suffix = "") -> std::string
  {
    std::shared_ptr<std::remove_cv_t<std::remove_reference_t<T>>> ptr;
    return hash(typeid(ptr), suffix);
  }

  /**
   * Provide a string that can be used as a hash for a type.
   */
  inline auto
  hash(const std::shared_ptr<std::any> &obj, const std::string &suffix = "")
    -> std::string
  {
    return hash(obj->type(), suffix);
  }

  /**
   * Provide a string that can be used as a hash for a type.
   */
  template <typename T>
  inline auto
  hash(const T && /*unused*/, const std::string &suffix = "") -> std::string
  {
    std::shared_ptr<std::remove_cv_t<std::remove_reference_t<T>>> ptr;
    return hash(typeid(ptr), suffix);
  }

  /**
   * Construct a pointer to a NodeObject from the arguments of the constructor.
   */
  template <typename... Args>
  inline auto
  make_node(Args &&...args) -> NodeObjectPtr
  {
    return std::make_shared<NodeObject>(args...);
  }

  /**
   * Construct a pointer to a NodeObject from a type T.
   */
  template <typename T>
  inline auto
  make_node() -> NodeObjectPtr
  {
    return std::make_shared<NodeObject>(hash<T>());
  }


  // Utility to detect if Arg is callable (can be wrapped by std::function)
  template <typename Arg, typename = void>
  struct is_callable : std::false_type
  {};

  template <typename Arg>
  struct is_callable<Arg,
                     std::void_t<decltype(std::function{std::declval<Arg>()})>>
    : std::true_type
  {};


  /**
   * Construct a pointer to a NodeObject for a named method
   */
  template <typename Arg>
  inline auto
  make_method_node(const std::string &method_name, Arg &&) -> NodeObjectPtr
  {
    std::string hash = "";
    if constexpr (is_callable<Arg>::value)
      {
        // If Arg is callable, hash the std::function type
        using FuncType = decltype(std::function{std::declval<Arg>()});
        hash           = coral::hash<FuncType>();
      }
    else
      {
        hash = coral::hash<Arg>();
      }
    return std::make_shared<NodeObject>(hash + method_name);
  }

  /**
   * The different types of connections that can be made between nodes.
   */
  enum class ConnectionType : unsigned int
  {
    //! Invalid connection type
    none = 0x000,
    //! Input only
    input = 0x001,
    //! Output only
    output = 0x002,
    //! Input and output
    pass_through = 0x003,
    //! Special connection to indicate that this input/output is the node itself
    self = 0x006,
  };

  /**
   * The different types of nodes.
   */
  enum class NodeType : unsigned int
  {
    //! The node has not been initialized with an object
    none,
    //! This is an abstract type. It will never be instantiated
    abstract,
    //! Trivially copyable and constructible types
    elementary_constructor,
    //! Non trivially copyable, but trivially constructible types
    empty_constructor,
    //! Non trivially copyable, and non trivially constructible types
    constructor,
    //! void member function
    void_method,
    //! void const member function
    void_const_method,
    //! non void member function
    method,
    //! non void const member function
    const_method,
    //! void function
    void_function,
    //! non void function
    function,
  };

  /**
   * Distinguish input arguments of a function from being input or input/output
   * arguments.
   */
  template <typename T>
  inline auto
  connection_type() -> ConnectionType
  {
    // For const reference types, we have input types
    if constexpr (std::is_reference_v<T> &&
                  std::is_const_v<std::remove_reference_t<T>>)
      return ConnectionType::input;
    // For non-const reference types, we have pass-through types
    else if constexpr (std::is_reference_v<T>)
      return ConnectionType::pass_through;
    else
      return ConnectionType::input;
  }

  /**
   * Store all std::functions that need to be used to build a NodeObject, and
   * the corresponding json serialization.
   */
  struct NodeObjectInitializer
  {
    /**
     * The type of the node.
     */
    NodeType node_type = NodeType::none;

    /**
     * The execution function associated with this node. This function will be
     * called when the node is executed. The behavior of this function depends
     * on the type of the node.
     */
    std::function<std::shared_ptr<std::any>(
      std::vector<std::shared_ptr<NodeObject>> args)>
      executor = [](std::vector<std::shared_ptr<NodeObject>>)
      -> std::shared_ptr<std::any> {
      std::cout << "empty node. ";
      return std::make_shared<std::any>();
    };

    /**
     * For supported types, we can also parse a string to a value.
     */
    std::function<std::shared_ptr<std::any>(std::string)> parse_string;

    /**
     * For supported types, we can also output the value as a string.
     */
    std::function<std::string(std::shared_ptr<std::any>)> to_string;

    /**
     * For derived types, we give a way to convert to the base class.
     */
    std::function<std::shared_ptr<std::any>(std::shared_ptr<std::any>)>
      to_base = [](std::shared_ptr<std::any> a) -> std::shared_ptr<std::any> {
      return a;
    };

    /**
     * The json serialization of the object->
     */
    mutable json json_serializer;
  };

  /**
   * @class NodeObject
   * @brief A class that represents an object of any type.
   *
   * The object itself is stored in a std::shared_ptr<std::any>. To allow for
   * serialization and to build non trivially constructible classes, the actual
   * type stored in std::shared_ptr<std::any> is a shared pointer to the
   * object.
   *
   * The object is built only when calling the operator() function. This allows
   * you to connect arguments to this object, in case the building of the object
   * requires other objects.
   */
  class NodeObject : public std::enable_shared_from_this<NodeObject>
  {
  public:
    NodeObject() = default;

    /**
     * Construct a new object from trivially constructible and copyable types.
     */
    template <typename T>
    NodeObject(const T &data)
      : NodeObject(std::make_shared<T>(data))
    {}

    /**
     * Construct NodeObject from a std::shared_ptr<std::any>. The
     * std::shared_ptr<std::any> is supposed to contain a shared pointer to a
     * registered type.
     */
    NodeObject(const std::shared_ptr<std::any> &data)
      : NodeObject(coral::hash(data))
    {}

    /**
     * Construct a new object from a shared pointer to a type.
     */
    template <typename T>
    NodeObject(std::shared_ptr<T> data)
    {
      object        = std::make_shared<std::any>(data);
      auto hash_str = coral::hash<T>();
      try
        {
          initializer = initializers.at(hash_str);
        }
      catch (const std::out_of_range &e)
        {
          throw std::runtime_error(
            "Type " + std::string(typeid(T).name()) +
            " is not registered. Before using it, you should call "
            "one of the NodeObject::register_*<" +
            std::string(typeid(T).name()) + ">(...) functions.");
        }

      initialize_inputs();
      initialize_outputs();
    }

    /**
     * Try to construct a new object from a hash string. If the hash is not
     * found, we store the actual string.
     */
    NodeObject(const std::string &hash_str)
    {
      try
        {
          initializer = initializers.at(hash_str);
        }
      catch (const std::out_of_range &e)
        {
          // If we did not find the hash, this is actually an std::string
          // object->
          *this = NodeObject(std::make_shared<std::string>(hash_str));
        }
      const auto &args   = initializer.json_serializer["arguments"];
      const auto  n_args = args.size();
      arguments.resize(n_args);
      connections.resize(n_args);
      for (unsigned int i = 0; i < n_args; ++i)
        {
          connections[i] = magic_enum::enum_cast<ConnectionType>(
                             args[i].at("connection_type").get<std::string>())
                             .value();
        }
      initialize_inputs();
      initialize_outputs();
    }

    /**
     * Build a NodeObject from a hash string.
     */
    NodeObject(const char *hash_str)
      : NodeObject(std::string(hash_str))
    {}

    /**
     * Return the registry of all types known to this class. If you try to
     * instantiate a class that is not in the registry, an exception will be
     * thrown.
     */
    static auto
    get_registry() -> json
    {
      json registry;
      for (const auto &[hash_str, initializer] : NodeObject::initializers)
        {
          registry[hash_str] = initializer.json_serializer;
        }
      return registry;
    }

    auto
    ready() const -> bool
    {
      return object.operator bool();
    }

    void
    parse_string(const std::string &value_str)
    {
      if (initializer.parse_string)
        object = initializer.parse_string(value_str);
      else
        {
          throw std::runtime_error(
            "No parse_string function is available for this type.");
        }
    }

    auto
    to_string() const -> std::string
    {
      if (initializer.to_string)
        {
          if (!ready())
            throw std::runtime_error("Object is not ready for conversion to "
                                     "string. Please call operator() first.");
          return initializer.to_string(object);
        }
      else
        {
          throw std::runtime_error("Unsupported type for JSON conversion");
        }
    }

    /**
     * Run the executor function of the object->
     *
     * @return true If the executor function was run successfully.
     * @return false If the executor function failed to run (i.e.,
     * if it failed when running or if some of the arguments were not ready.)
     */
    auto
    operator()() -> bool
    {
      bool is_ready = true;
      for (const auto &arg : arguments)
        {
          if (!arg->ready())
            {
              is_ready = false;
              break;
            }
        }
      if (!is_ready)
        throw std::runtime_error(
          "Arguments are not ready. You can only call "
          "this function after all arguments are ready.");

      object = initializer.executor(arguments);
      // Check if we have to copy back the original value
      if (initializer.json_serializer.contains("value") &&
          initializer.parse_string)
        object = initializer.parse_string(
          initializer.json_serializer["value"].template get<std::string>());
      return object.operator bool();
    }

    /**
     * Set the arguments of the node executor.
     */
    void
    set_arguments(const std::vector<std::shared_ptr<NodeObject>> &args)
    {
      if (args.size() != initializer.json_serializer["arguments"].size())
        throw std::runtime_error(
          "Wrong number of arguments: " + std::to_string(args.size()) +
          " instead of " +
          std::to_string(initializer.json_serializer["arguments"].size()) +
          ".");
      this->arguments = args;
    }

    /**
     * Register a new type in the json registry. This method does not add
     * constructors.
     */
    template <typename T>
    static auto
    register_json_header(const std::string &suffix = "")
      -> NodeObjectInitializer &
    {
      auto hash_str = coral::hash<T>(suffix);
      if (initializers.find(hash_str) != initializers.end())
        // Reset the initializer
        initializers[hash_str] = {};

      auto &initializer                        = initializers[hash_str];
      initializer.json_serializer["type"]      = boost::core::type_name<T>();
      initializer.json_serializer["type_hash"] = hash_str;
      initializer.json_serializer["arguments"] = json::array();
      initializer.json_serializer["inputs"]    = json::array();
      initializer.json_serializer["outputs"]   = json::array();
      return initializer;
    }

    /**
     * Register a new type in the json registry, for types that require
     * additional arguments. This method does not add constructors.
     */
    template <typename T, typename... Args>
    static auto
    register_json_header(const std::vector<std::string> &arg_names,
                         const std::string              &suffix = "")
      -> NodeObjectInitializer &
    {
      auto &initializer = register_json_header<T>(suffix);

      // Now take care of the arguments.
      std::vector<std::shared_ptr<NodeObject>> args = {std::make_shared<
        NodeObject>(
        std::shared_ptr<std::remove_cv_t<std::remove_reference_t<Args>>>())...};

      std::vector<ConnectionType> arg_connection_types = {
        connection_type<Args>()...};

      if (args.size() != arg_names.size())
        throw std::runtime_error("Wrong number of arguments.");

      for (unsigned int i = 0; i < args.size(); ++i)
        {
          initializer.json_serializer["arguments"][i]["name"] = arg_names[i];
          initializer.json_serializer["arguments"][i]["type"] =
            args[i]->type_name();
          initializer.json_serializer["arguments"][i]["type_hash"] =
            args[i]->hash();
          initializer.json_serializer["arguments"][i]["connection_type"] =
            magic_enum::enum_name(arg_connection_types[i]);
          if ((arg_connection_types[i] & ConnectionType::input) !=
              ConnectionType::none)
            initializer.json_serializer["inputs"].push_back(i);
          if ((arg_connection_types[i] & ConnectionType::output) !=
              ConnectionType::none)
            initializer.json_serializer["outputs"].push_back(i);
        }
      return initializer;
    }

    /**
     * Register an elementary type. This is a type that does not require any
     * arguments to be constructed, it is trivially copyable, and its values
     * can be deduced from a string using
     * dealii::Patterns::Tools::parse_string().
     */
    template <typename T>
    static auto
    register_elementary_type() -> NodeObjectInitializer &
    {
      auto &initializer                        = register_json_header<T>();
      initializer.json_serializer["node_type"] = "elementary_constructor";
      initializer.node_type = NodeType::elementary_constructor;

      // Special handling for std::string
      if constexpr (std::is_same_v<T, std::string>)
        {
          initializer.json_serializer["value"] = ""; // Empty string default
        }
      else
        {
          initializer.json_serializer["value"] = json(T()).dump();
        }

      initializer.json_serializer["outputs"].push_back(-1);

      // Add value parser
      initializer.parse_string =
        [](const std::string &value) -> std::shared_ptr<std::any> {
        auto t = std::make_shared<T>();

        if constexpr (std::is_same_v<T, std::string>)
          {
            *t = value;
          }
        else
          {
            // For non-string types, use standard JSON parsing
            *t = json::parse(value).get<T>();
          }

        return std::make_shared<std::any>(t);
      };

      // Add to the initializer the emtpy executor.
      initializer.executor =
        [](const std::vector<std::shared_ptr<NodeObject>> &args)
        -> std::shared_ptr<std::any> {
        if (args.size() != 0)
          throw std::runtime_error("Wrong number of arguments.");
        return std::make_shared<std::any>(std::make_shared<T>());
      };

      // Add value parser
      initializer.to_string =
        [](const std::shared_ptr<std::any> &value) -> std::string {
        const T &t = *std::any_cast<std::shared_ptr<T>>(*value);

        if constexpr (std::is_same_v<T, std::string>)
          {
            return t;
          }
        else
          {
            // For non-string types, use standard JSON serialization
            return json(t).dump();
          }
      };
      return initializer;
    }

    /**
     * Register a trivially constructible type. This is a type that does not
     * require any arguments to be constructed, but it is not trivially
     * copyable. This is the case for example of dealii::Triangulation<2>.
     */
    template <typename T>
    static auto
    register_type() -> NodeObjectInitializer &
    {
      auto &initializer                        = register_json_header<T>();
      initializer.json_serializer["node_type"] = "empty_constructor";
      initializer.node_type                    = NodeType::empty_constructor;
      initializer.json_serializer["outputs"].push_back(-1);

      // Add to the initializer the emtpy executor.
      initializer.executor =
        [](const std::vector<std::shared_ptr<NodeObject>> &args)
        -> std::shared_ptr<std::any> {
        if (args.size() != 0)
          throw std::runtime_error("Wrong number of arguments.");
        return std::make_shared<std::any>(std::make_shared<T>());
      };
      return initializer;
    }

    /**
     * Register an abstract type. This is a type that will never be
     * constructed.
     */
    template <typename T>
    static auto
    register_abstract_type() -> NodeObjectInitializer &
    {
      auto &initializer                        = register_json_header<T>();
      initializer.json_serializer["node_type"] = "abstract";
      initializer.node_type                    = NodeType::abstract;

      // Add to the initializer the emtpy executor.
      initializer.executor =
        [&initializer](std::vector<std::shared_ptr<NodeObject>>)
        -> std::shared_ptr<std::any> {
        std::cout << "empty_constructor: "
                  << initializer.json_serializer["type"] << std::endl;
        return std::make_shared<std::any>(std::shared_ptr<T>());
      };
      return initializer;
    }

    /**
     * Register a trivially constructible type T, derived from type B.
     *
     * If a function requires a base class, and you want to pass a derived
     * class, you can register the derived class with this function. This will
     * allow the function to accept the derived class as an argument.
     */
    template <typename B, typename T>
    static auto
    register_derived_type() -> NodeObjectInitializer &
    {
      auto &initializer = register_type<T>();
      initializer.json_serializer["outputs"].push_back(-1);

      auto &base_initializer = register_abstract_type<B>();

      base_initializer.json_serializer["derived"].push_back(
        initializer.json_serializer["type_hash"]);

      initializer.json_serializer["base"] =
        base_initializer.json_serializer["type_hash"];

      // Add the conversion to the base class.
      initializer.to_base =
        [](std::shared_ptr<std::any> a) -> std::shared_ptr<std::any> {
        std::cout << "Cast to derived class " << boost::core::type_name<T>()
                  << "\n";
        auto ptr = std::any_cast<std::shared_ptr<T>>(*a);
        std::cout << "Cast to base class " << boost::core::type_name<B>()
                  << "\n";
        auto ptrB = std::static_pointer_cast<B>(ptr);
        std::cout << "Return std::any of base class"
                  << "\n";
        return std::make_shared<std::any>(ptrB);
      };
      return initializer;
    }

    /**
     * Register a non-trivially constructible type. This is a type that does
     * require arguments to be constructed, and that it is not trivially
     * copyable. This is the case for example of dealii::FE_Q<2>(), that
     * requires the degree of the finite element to be instantiated.
     */
    template <typename T, typename... Args>
    static auto
    register_type(const std::vector<std::string> &arg_names)
      -> NodeObjectInitializer &
    {
      auto &initializer = register_json_header<T, Args...>(arg_names);
      initializer.json_serializer["node_type"] = "constructor";
      initializer.node_type                    = NodeType::constructor;
      initializer.json_serializer["outputs"].push_back(-1);

      // And the executor.
      initializer.executor =
        [&initializer](std::vector<std::shared_ptr<NodeObject>> args)
        -> std::shared_ptr<std::any> {
        if (args.size() != sizeof...(Args))
          throw std::runtime_error("Wrong number of arguments.");
        auto tuple = cast_args<Args...>(args);
        std::cout << "constructor: " << initializer.json_serializer["type"]
                  << std::endl;
        return std::apply(
          [](auto &&...unpackedArgs) {
            return std::make_shared<std::any>(
              std::make_shared<T>(*unpackedArgs...));
          },
          tuple);
      };
      return initializer;
    }

    /**
     * Register a non-trivially constructible type T derived from type B.
     */
    template <typename B, typename T, typename... Args>
    static auto
    register_derived_type(const std::vector<std::string> &arg_names)
      -> NodeObjectInitializer &
    {
      auto &initializer      = register_type<T, Args...>(arg_names);
      auto &base_initializer = register_abstract_type<B>();

      base_initializer.json_serializer["derived"].push_back(
        initializer.json_serializer["type_hash"]);

      initializer.json_serializer["base"] =
        base_initializer.json_serializer["type_hash"];

      // Add the conversion to the base class.
      initializer.to_base =
        [](std::shared_ptr<std::any> a) -> std::shared_ptr<std::any> {
        return std::make_shared<std::any>(
          std::static_pointer_cast<B>(std::any_cast<std::shared_ptr<T>>(*a)));
      };
      return initializer;
    }

    /**
     * Same as above, for objects that require a single argument.
     */
    template <typename T, typename Arg>
    static auto
    register_type(const std::string &arg_name) -> NodeObjectInitializer &
    {
      return register_type<T, Arg>(std::vector<std::string>{{arg_name}});
    }

    /**
     * Same as above, for objects that require a single argument.
     */
    template <typename B, typename T, typename Arg>
    static auto
    register_derived_type(const std::string &arg_name)
      -> NodeObjectInitializer &
    {
      return register_derived_type<B, T, Arg>(
        std::vector<std::string>{{arg_name}});
    }

    template <typename T, typename ReturnType, typename... Args>
    using MethodPtr = ReturnType (T::*)(Args...);

    template <typename T, typename ReturnType, typename... Args>
    using ConstMethodPtr = ReturnType (T::*)(Args...) const;

    /**
     * Register a non-const method of a class. This node will have as
     * arguments the object of the class, possibly the output argument, and
     * the arguments of the method.
     *
     * The method can be void or return a value, and can have any number of
     * arguments.
     *
     * The first argument of the method is the object of the class, and the
     * @p arg_names must reflect this, i.e.,
     * @p arg_names[0] is the name by which we store this function name in the
     * json serializer,  @p arg_names[1] is the name of the class
     * @p arg_names[2] must be the (optional) name we give to
     * the output argument, and the rest of the arguments are the names of the
     * arguments of the method.
     */
    template <typename T, typename ReturnType, typename... Args>
    static void
    register_method(MethodPtr<T, ReturnType, Args...> ptr,
                    std::vector<std::string>          arg_names)
    {
      using ThisMethod              = MethodPtr<T, ReturnType, Args...>;
      constexpr bool return_is_void = std::is_same_v<ReturnType, void>;
      auto           method_name    = arg_names[0];
      arg_names.erase(arg_names.begin());

      if constexpr (return_is_void)
        {
          auto &initializer =
            register_json_header<ThisMethod, T &, Args...>(arg_names,
                                                           method_name);
          initializer.json_serializer["node_type"]   = "void_method";
          initializer.node_type                      = NodeType::void_method;
          initializer.json_serializer["method_name"] = method_name;

          // Add the method to the initializer
          initializer.executor =
            [ptr, &initializer](std::vector<std::shared_ptr<NodeObject>> args)
            -> std::shared_ptr<std::any> {
            if (args.size() != 1 + sizeof...(Args))
              throw std::runtime_error("Wrong number of arguments.");
            auto &obj = args[0]->get<T>();
            args.erase(args.begin()); // remove the first element

            auto tuple = cast_args<Args...>(args);
            std::cout << initializer.json_serializer["method_name"] << ": "
                      << initializer.json_serializer["type"] << std::endl;
            std::apply(
              [&obj, ptr](auto &&...unpackedArgs) {
                (obj.*ptr)(*unpackedArgs...);
              },
              tuple);
            return std::make_shared<std::any>(ptr);
          };
        }
      else
        {
          auto &initializer =
            register_json_header<ThisMethod, T &, ReturnType &, Args...>(
              arg_names, method_name);
          initializer.json_serializer["node_type"]   = "method";
          initializer.node_type                      = NodeType::method;
          initializer.json_serializer["method_name"] = method_name;

          // Add to the initializer the emtpy executor.
          initializer.executor =
            [ptr, &initializer](std::vector<std::shared_ptr<NodeObject>> args)
            -> std::shared_ptr<std::any> {
            if (args.size() != 2 + sizeof...(Args))
              throw std::runtime_error("Wrong number of arguments.");
            auto &obj = args[0]->get<T>();
            auto &ret = args[1]->get<ReturnType>();

            // Create a copy of args without the first two elements for function
            // execution
            std::vector<std::shared_ptr<NodeObject>> function_args(
              args.begin() + 2, args.end());

            std::cout << initializer.json_serializer["method_name"] << ": "
                      << initializer.json_serializer["type"] << std::endl;
            auto tuple = cast_args<Args...>(function_args);
            ret        = std::apply(
              [&obj, ptr](auto &&...unpackedArgs) {
                return (obj.*ptr)(*unpackedArgs...);
              },
              tuple);
            return std::make_shared<std::any>(ptr);
          };
        }
    }

    /**
     * Register a const method of a class. This node will have as arguments
     * the object of the class, possibly the output argument, and the
     * arguments of the method.
     *
     * The method can be void or return a value, and can have any number of
     * arguments, but it must be a const method of the class.
     *
     * The first argument of the method is the object of the class, and the
     * @p arg_names must reflect this, i.e.,
     * @p arg_names[0] is the name by which we store this function name in the
     * json serializer, @p arg_names[1] must be the (optional) name we give to
     * the output argument, and the rest of the arguments are the names of the
     * arguments of the method.
     */
    template <typename T, typename ReturnType, typename... Args>
    static void
    register_method(ConstMethodPtr<T, ReturnType, Args...> ptr,
                    std::vector<std::string>               arg_names)
    {
      using ThisMethod              = ConstMethodPtr<T, ReturnType, Args...>;
      constexpr bool return_is_void = std::is_same_v<ReturnType, void>;
      auto           method_name    = arg_names[0];
      arg_names.erase(arg_names.begin());

      if constexpr (return_is_void)
        {
          auto &initializer =
            register_json_header<ThisMethod, const T &, Args...>(arg_names,
                                                                 method_name);
          initializer.json_serializer["node_type"] = "void_const_method";
          initializer.node_type = NodeType::void_const_method;
          initializer.json_serializer["method_name"] = method_name;

          // Add the method to the initializer
          initializer.executor =
            [ptr, &initializer](std::vector<std::shared_ptr<NodeObject>> args)
            -> std::shared_ptr<std::any> {
            if (args.size() != 1 + sizeof...(Args))
              throw std::runtime_error("Wrong number of arguments.");
            auto &obj = args[0]->get<T>();
            args.erase(args.begin()); // remove the first element

            auto tuple = cast_args<Args...>(args);
            std::cout << "method: " << initializer.json_serializer["type"]
                      << std::endl;
            std::apply(
              [&obj, ptr](auto &&...unpackedArgs) {
                (obj.*ptr)(*unpackedArgs...);
              },
              tuple);
            return std::make_shared<std::any>(ptr);
          };
        }
      else
        {
          auto &initializer =
            register_json_header<ThisMethod, const T &, ReturnType &, Args...>(
              arg_names, method_name);
          initializer.json_serializer["node_type"]   = "const_method";
          initializer.node_type                      = NodeType::const_method;
          initializer.json_serializer["method_name"] = method_name;

          // Add to the initializer the emtpy executor.
          initializer.executor =
            [ptr, &initializer](std::vector<std::shared_ptr<NodeObject>> args)
            -> std::shared_ptr<std::any> {
            if (args.size() != 2 + sizeof...(Args))
              throw std::runtime_error("Wrong number of arguments.");
            auto &obj = args[0]->get<T>();
            auto &ret = args[1]->get<ReturnType>();
            args.erase(args.begin()); // remove the class
            args.erase(args.begin()); // remove the return type
            std::cout << "method: " << initializer.json_serializer["type"]
                      << std::endl;
            auto tuple = cast_args<Args...>(args);
            ret        = std::apply(
              [&obj, ptr](auto &&...unpackedArgs) {
                (obj.*ptr)(*unpackedArgs...);
              },
              tuple);
            return std::make_shared<std::any>(ptr);
          };
        }
    }

    /**
     * Register a free function. This node will have as arguments the
     * output of the function, and the arguments of the method.
     *
     * The function can be void or return a value, and can have any number of
     * arguments.
     *
     * The first argument of the method is the return type of the class, and
     * the
     * @p arg_names must reflect this if non-void, i.e.,
     * @p arg_names[0] is the name of the function, while @p arg_names[1] is the
     * (optional) output node (if non-void) by which we store this
     * function name, while the rest of the arguments are the names of the
     * arguments of the method.
     */
    template <typename ReturnType, typename... Args>
    static void
    register_function(std::function<ReturnType(Args...)> ptr,
                      std::vector<std::string>           arg_names)
    {
      using ThisMethod              = decltype(ptr);
      constexpr bool return_is_void = std::is_same_v<ReturnType, void>;
      if (arg_names.empty())
        throw std::runtime_error("You must provide at least the name of the "
                                 "function as the first argument.");
      auto method_name = arg_names[0];
      arg_names.erase(arg_names.begin());

      if constexpr (return_is_void)
        {
          if (arg_names.size() != sizeof...(Args))
            throw std::runtime_error("Wrong number of arguments.");
          auto &initializer =
            register_json_header<ThisMethod, Args...>(arg_names, method_name);
          initializer.json_serializer["node_type"]   = "void_function";
          initializer.node_type                      = NodeType::void_function;
          initializer.json_serializer["method_name"] = method_name;

          // Add the method to the initializer
          initializer.executor =
            [ptr, &initializer](std::vector<std::shared_ptr<NodeObject>> args)
            -> std::shared_ptr<std::any> {
            if (args.size() != sizeof...(Args))
              throw std::runtime_error("Wrong number of arguments.");

            auto tuple = cast_args<Args...>(args);
            std::cout << "void function: "
                      << initializer.json_serializer["type"] << std::endl;
            std::apply([ptr](auto &&...unpackedArgs) { ptr(*unpackedArgs...); },
                       tuple);
            return std::make_shared<std::any>(ptr);
          };
        }
      else
        {
          // Treat output sepearately, as it is the first argument
          auto &initializer =
            register_json_header<ThisMethod, ReturnType &, Args...>(
              arg_names, method_name);
          initializer.json_serializer["node_type"]   = "function";
          initializer.node_type                      = NodeType::function;
          initializer.json_serializer["method_name"] = method_name;

          // Add the method to the initializer
          initializer.executor =
            [ptr, &initializer](std::vector<std::shared_ptr<NodeObject>> args)
            -> std::shared_ptr<std::any> {
            if (args.size() != 1 + sizeof...(Args))
              throw std::runtime_error("Wrong number of arguments.");
            auto &ret = args[0]->get<ReturnType>();
            args.erase(args.begin()); // remove the first element

            auto tuple = cast_args<Args...>(args);
            std::cout << "non-void function: "
                      << initializer.json_serializer["type"] << std::endl;
            ret = std::apply(
              [ptr](auto &&...unpackedArgs) { return ptr(*unpackedArgs...); },
              tuple);
            return std::make_shared<std::any>(ptr);
          };
        }
    }

    // Overload for raw function pointers
    template <typename ReturnType, typename... Args>
    static void
    register_function(ReturnType (*ptr)(Args...),
                      std::vector<std::string> arg_names)
    {
      std::function<ReturnType(Args...)> func = ptr;
      register_function(func, arg_names);
    }

    // Helper to deduce template arguments automatically
    template <typename Lambda>
    static void
    register_function(Lambda func, std::vector<std::string> arg_names)
    {
      auto f = std::function{func};
      register_function(f, arg_names);
    }

    /**
     * Get a shared pointer of the stored object->
     *
     * Will throw if the object is not ready, or if the stored object is not
     * of the requested type.
     */
    template <typename T>
    auto
    get_shared() -> std::shared_ptr<T>
    {
      if (!ready())
        throw std::runtime_error("Object is not ready.");
      using type = std::remove_cv_t<std::remove_reference_t<T>>;
      std::shared_ptr<type> ptr;

      if (hash() != coral::hash<type>())
        {
          // If the object is not of the requested type, try to convert it
          // to the base class, using the right initializer.
          auto &j = initializer.json_serializer;
          if (!(j.contains("base") &&
                (coral::hash<type>() == j.at("base").get<std::string>())))
            throw std::runtime_error("Cannot cast object of type " +
                                     type_name() + " to object of type " +
                                     boost::core::type_name<type>() + ".");
          auto new_object = initializer.to_base(object);
          if (!new_object->has_value())
            throw std::runtime_error("New object does not have value.");
          if (!(coral::hash(new_object) == j.at("base").get<std::string>()))
            throw std::runtime_error(
              "New object does not have the right hash.");
          ptr = std::any_cast<std::shared_ptr<type>>(*new_object);
        }
      else
        try
          {
            ptr = std::any_cast<std::shared_ptr<type>>(*object);
          }
        catch (...)
          {
            throw std::runtime_error(
              "Could not cast object to shared pointer of type " +
              boost::core::type_name<type>() + " from object of type " +
              boost::core::demangle(object->type().name()) + ".");
          }
      return ptr;
    }

    /**
     * Get a writeable reference to the stored object->
     */
    template <typename T>
    T &
    get()
    {
      if (!ready())
        throw std::runtime_error("Object is not ready.");
      return *(get_shared<T>());
    }

    /**
     * Get a const reference to the stored object->
     */
    template <typename T>
    const T &
    get() const
    {
      if (!ready())
        throw std::runtime_error("Object is not ready.");
      return *(get_shared<T>());
    }

    /**
     * Expose the underlying std::shared_ptr<std::any> object->
     */
    operator const std::shared_ptr<std::any> &() const
    {
      return object;
    }

    /**
     * Return a Pointer to the ith output.
     */
    NodeObjectPtr
    output(const unsigned int index)
    {
      if (!(index < output_indices.size()))
        throw std::runtime_error("Index out of bounds.");
      if (output_indices[index] == -1)
        {
          return shared_from_this(); // Return this object for 'self'
        }
      return arguments[output_indices[index]];
    }

    /**
     * Return a Pointer to the ith output.
     */
    void
    set_output(const unsigned int index, const NodeObjectPtr &value)
    {
      if (!(index < output_indices.size()))
        throw std::runtime_error("Index out of bounds.");
      if (output_indices[index] == -1)
        throw std::runtime_error("Cannot set output for 'self'.");

      arguments[output_indices[index]] = value;
    }

    /**
     * Return a Pointer to the @p index -th input.
     */
    NodeObjectPtr
    input(const unsigned int index)
    {
      if (!(index < input_indices.size()))
        throw std::runtime_error(
          "Index out of bounds: you asked for input " + std::to_string(index) +
          ", but there are " + std::to_string(input_indices.size()) +
          " inputs: " + initializer.json_serializer.dump(2));
      if (input_indices[index] == -1)
        return shared_from_this(); // Return this object for 'self'
      else
        {
          auto arg_id = input_indices[index];
          if (!(arg_id < static_cast<int>(arguments.size())))
            throw std::runtime_error(
              "Internal error! Input you asked for input " +
              std::to_string(index) + ", which points to argument number " +
              std::to_string(arg_id) + ", but there are only " +
              std::to_string(arguments.size()) + " arguments to pick from.");
          return arguments[input_indices[index]];
        }
    }

    void
    set_input(const unsigned int index, const NodeObjectPtr &value)
    {
      if (!(index < input_indices.size()))
        throw std::runtime_error(
          "Index out of bounds: you asked for input " + std::to_string(index) +
          ", but there are " + std::to_string(input_indices.size()) +
          " inputs: " + initializer.json_serializer.dump(2));
      if (input_indices[index] == -1)
        throw std::runtime_error("Cannot set input for 'self'.");
      else
        {
          auto arg_id = input_indices[index];
          if (!(arg_id < static_cast<int>(arguments.size())))
            throw std::runtime_error(
              "Internal error! You asked to set input " +
              std::to_string(index) + ", which points to argument number " +
              std::to_string(arg_id) + ", but there are only " +
              std::to_string(arguments.size()) + " arguments to pick from.");
          arguments[input_indices[index]] = value;
        }
    }

    /**
     * Expose the underlying std::shared_ptr<std::any> object->
     */
    operator std::shared_ptr<std::any> &()
    {
      return object;
    }

    template <typename T>
    std::shared_ptr<const T>
    get_shared() const
    {
      using type = std::remove_cv_t<std::remove_reference_t<T>>;
      std::shared_ptr<const type> ptr;
      if (hash() != coral::hash<type>())
        {
          // If the object is not of the requested type, try to convert it
          // to the base class, using the right initializer.
          auto &j = initializer.json_serializer;
          if (!(j.contains("base") &&
                (coral::hash<type>() == j.at("base").get<std::string>())))
            throw std::runtime_error("Cannot cast object of type " +
                                     type_name() + " to object of type " +
                                     boost::core::type_name<type>() + ".");
          auto new_object = initializer.to_base(object);
          if (!new_object->has_value())
            throw std::runtime_error("New object does not have value.");
          if (!(coral::hash(new_object) == j.at("base").get<std::string>()))
            throw std::runtime_error(
              "New object does not have the right hash.");
          ptr = std::any_cast<std::shared_ptr<const type>>(new_object);
        }
      else
        try
          {
            ptr = std::any_cast<std::shared_ptr<const type>>(object);
          }
        catch (...)
          {
            throw std::runtime_error(
              "Could not cast object to shared pointer of type " +
              boost::core::type_name<type>() + " from object of type " +
              boost::core::demangle(object->type().name()) + ".");
          }
      return ptr;
    }

    template <typename T>
    NodeObject &
    operator=(std::shared_ptr<T> data)
    {
      if (!object->has_value())
        {
          *this = NodeObject(data);
          return *this;
        }
      else
        {
          // Check that we store a compatible type first.
          // Check that coral::hash<T>() is contained in hash().
          const auto my_hash   = hash();
          const auto type_hash = coral::hash<T>();
          if (my_hash.find(type_hash) != 0)
            throw std::runtime_error(
              "Object type does not match. My hash is " + my_hash +
              " and the object hash is " + type_hash +
              ". They should at least start with the same characters.");
          *this = NodeObject(data);
          return *this;
        }
    }

    template <typename T>
    NodeObject &
    operator=(const T &data)
    {
      *this = NodeObject(std::make_shared<T>(data));
      return *this;
    }

    const json &
    get_info() const
    {
      auto &j = initializer.json_serializer;
      if (initializer.to_string && object->has_value())
        j["value"] = initializer.to_string(object);
      return j;
    }

    /**
     * Get the hash of the stored object->
     */
    std::string
    hash() const
    {
      if (object.operator bool())
        {
          // The object is initialized. Check if this is consistent with
          // the initializer, and return its hash.
          const auto object_hash = coral::hash(object->type());
          const auto stored_hash =
            std::string(initializer.json_serializer.at("type_hash"));
          if (stored_hash.find(object_hash) != 0)
            throw std::runtime_error(
              "Object type does not match: we store " + stored_hash +
              ", and cannot set this object equal to " + object_hash +
              ". The two hashes should at least start with the same characters.");
          return stored_hash;
        }
      else
        {
          return initializer.json_serializer.at("type_hash");
        }
    }

    std::string
    type_name() const
    {
      return initializer.json_serializer.at("type");
    }

    NodeType
    node_type() const
    {
      return magic_enum::enum_cast<NodeType>(
               initializer.json_serializer.at("node_type").get<std::string>())
        .value();
    }

    /**
     * Get the number of arguments that this node has.
     */
    size_t
    n_arguments() const
    {
      return initializer.json_serializer["arguments"].size();
    }

    /**
     * Get the number of inputs.
     */
    size_t
    n_inputs() const
    {
      return initializer.json_serializer["inputs"].size();
    }

    /**
     * Get the number of outputs.
     */
    size_t
    n_outputs() const
    {
      return initializer.json_serializer["outputs"].size();
    }

  private:
    /**
     * The actual object is stored here as a std::shared_ptr<std::any>.
     */
    std::shared_ptr<std::any> object;

    /**
     * Anything required to build a std::shared_ptr<T> object, and to
     * manipulate it.
     */
    NodeObjectInitializer initializer;

    /**
     * The arguments to pass to the executor.
     */
    std::vector<std::shared_ptr<NodeObject>> arguments;

    /**
     * Arguments to connection type.
     */
    std::vector<ConnectionType> connections;

    /**
     * A list of all known types and their initializers.
     */
    static std::map<std::string, NodeObjectInitializer> initializers;

    /**
     * Input indices mapping to arguments.
     */
    std::vector<int> input_indices;

    /**
     * Output indices mapping to arguments. If the content is -1, then we
     * return the object itself.
     */
    std::vector<int> output_indices;

    void
    initialize_inputs()
    {
      const auto &json_inputs = initializer.json_serializer["inputs"];
      input_indices.resize(json_inputs.size());
      for (unsigned int i = 0; i < json_inputs.size(); ++i)
        {
          input_indices[i] = json_inputs[i].get<unsigned int>();
        }
    }

    void
    initialize_outputs()
    {
      const auto &json_outputs = initializer.json_serializer["outputs"];
      output_indices.resize(json_outputs.size());
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
                  .json_serializer["arguments"][output_indices[i]]["type_hash"]
                  .get<std::string>());
            }
        }
    }

  public:
    void
    set_inputs(
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
            initializer
              .json_serializer["arguments"][input_indices[i]]["type_hash"]
              .get<std::string>();
          const auto input_hash = input_node->output(input_index)->hash();

          // Check if the input hash matches the expected hash or is derived
          // from the base type
          const auto &base_hash =
            initializer.json_serializer["arguments"][input_indices[i]].value(
              "base", "");
          const bool is_valid =
            (expected_hash == input_hash) || (base_hash == input_hash);

          if (!is_valid)
            throw std::runtime_error(
              "The hash type of input " + std::to_string(i) + " (" +
              input_hash + ") does not match the expected hash (" +
              expected_hash + ") or its base type (" + base_hash + ".");

          arguments[input_indices[i]] = input_node->output(input_index);
        }
    }

    void
    set_outputs(
      const std::vector<std::pair<NodeObjectPtr, unsigned int>> &outputs)
    {
      if (outputs.size() != output_indices.size())
        throw std::runtime_error(
          "Wrong number of outputs: " + std::to_string(outputs.size()) +
          " instead of " + std::to_string(output_indices.size()) + ".");

      for (unsigned int i = 0; i < outputs.size(); ++i)
        {
          const auto &output_node  = outputs[i].first;
          const auto &output_index = outputs[i].second;

          const auto expected_hash =
            initializer
              .json_serializer["arguments"][output_indices[i]]["type_hash"]
              .get<std::string>();
          const auto output_hash = arguments[output_indices[i]]->hash();

          if (expected_hash != output_hash)
            throw std::runtime_error("The hash type of output " +
                                     std::to_string(i) + " (" + output_hash +
                                     ") does not match the expected hash (" +
                                     expected_hash + ").");

          output_node->arguments[output_index] = arguments[output_indices[i]];
        }
    }
  };

  /**
   * json serialization of a Nodeobject->
   */
  inline void
  to_json(json &j, const NodeObjectPtr &obj)
  {
    j = obj->get_info();
  }

  /**
   * json deserialization of a Nodeobject->
   */
  inline void
  from_json(const json &j, NodeObjectPtr &obj)
  {
    if (!j.contains("type_hash"))
      throw std::runtime_error(
        "The json does not contain a type_hash entry. Bailing out.");
    obj = make_node(j.at("type_hash").get<std::string>());
    // Make sure the hash matches the expected value
    if (j["type_hash"] != obj->hash())
      throw std::runtime_error(
        "The type_hash does not match the expected value: expected " +
        j["type_hash"].dump() + ", got " + obj->hash());

    if (j["node_type"] == "elementary_constructor" ||
        j["node_type"] == "empty_constructor")
      (*obj)();
    if (j.contains("value"))
      {
        obj->parse_string(j.at("value").get<std::string>());
      }
    auto j2 = obj->get_info();
    if (!utils::is_json_subset_of(j2, j))
      {
        throw std::runtime_error(
          "The json object does not match the expected value: expected " +
          j.dump(2) + ", got " + j2.dump(2) + ".");
      }
  }

  /**
   * Implementation of cast_args function.
   */
  template <typename... Args, std::size_t... Is>
  inline std::tuple<
    std::shared_ptr<std::remove_const_t<std::remove_reference_t<Args>>>...>
  cast_args_impl(const std::vector<std::shared_ptr<NodeObject>> &args,
                 std::index_sequence<Is...>)
  {
    try
      {
        return std::make_tuple(
          args[Is]
            ->get_shared<
              std::remove_const_t<std::remove_reference_t<Args>>>()...);
      }
    catch (const std::bad_any_cast &e)
      {
        throw std::runtime_error(e.what());
      }
  }

  /**
   * Cast a vector of NodeObject arguments to a tuple of shared pointers.
   */
  template <typename... Args>
  inline std::tuple<
    std::shared_ptr<std::remove_const_t<std::remove_reference_t<Args>>>...>
  cast_args(const std::vector<std::shared_ptr<NodeObject>> &args)
  {
    return cast_args_impl<Args...>(args, std::index_sequence_for<Args...>{});
  }

  /**
   * Connect the inputs of a node.
   *
   * This function sets the inputs of @p node to the output connectors of
   * nodes in @p inputs, in particular, it sets the input of the node to
   * @p inputs[i].first->outputs[inputs[i].second].
   *
   * @param node The node to connect
   * @param inputs The pair input/output connector of the node
   */
  inline void
  connect(
    NodeObjectPtr                                             &node,
    const std::vector<std::pair<NodeObjectPtr, unsigned int>> &inputs = {})
  {
    node->set_inputs(inputs);
  }
} // namespace coral
#endif
