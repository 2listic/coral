#ifndef CORAL_H
#define CORAL_H

#include <nlohmann/json.hpp> // JSON library

#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include "slog.h"

#if defined(__GNUC__)
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wmissing-braces"
#endif
#include <entt/entt.hpp>
#if defined(__GNUC__)
#  pragma GCC diagnostic pop
#endif
#include "magic_enum/magic_enum_all.hpp"
#include "type_name.h"
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
  class Network;

  using NodeObjectPtr = std::shared_ptr<NodeObject>;

  namespace detail
  {
    template <typename... Args>
    inline std::tuple<
      std::shared_ptr<std::remove_const_t<std::remove_reference_t<Args>>>...>
    cast_args(const std::vector<NodeObjectPtr> &args);

    std::shared_ptr<entt::meta_any> &
    meta_any_ref(NodeObject &node);

    const std::shared_ptr<entt::meta_any> &
    meta_any_ref(const NodeObject &node);

    // Thread-safety: registration/aliasing is not thread-safe; do this
    // single-threaded before concurrent use.
    inline auto
    type_aliases() -> std::unordered_map<std::size_t, std::string> &
    {
      static std::unordered_map<std::size_t, std::string> aliases;
      return aliases;
    }

    inline auto
    type_aliases_mutex() -> std::mutex &
    {
      static std::mutex m;
      return m;
    }

    // Thread-safety: identifiers are stored in a shared static set without
    // synchronization.
    inline const char *
    store_identifier(const std::string &id)
    {
      static std::mutex                 mutex;
      static std::set<std::string>      identifiers;
      const std::lock_guard<std::mutex> lock(mutex);
      return identifiers.insert(id).first->c_str();
    }



    inline auto
    type_identifier(const entt::meta_type &type, const std::string &suffix = "")
      -> std::string
    {
      if (!type)
        return suffix;

      const char       *name = type.name();
      const std::string base = (name != nullptr && name[0] != '\0') ?
                                 std::string(name) :
                                 std::string(type.info().name());
      return base + suffix;
    }



    template <typename Base, typename Derived>
    std::shared_ptr<Base>
    shared_ptr_to_base(const std::shared_ptr<Derived> &ptr)
    {
      return std::static_pointer_cast<Base>(ptr);
    }

    /** \cond INTERNAL */
    // Utility to detect if Arg is callable (can be wrapped by std::function).
    template <typename Arg, typename = void>
    struct is_callable : std::false_type
    {};

    template <typename Arg>
    struct is_callable<
      Arg,
      std::void_t<decltype(std::function{std::declval<Arg>()})>>
      : std::true_type
    {};

    /** \endcond */
  } // namespace detail

  namespace detail
  {
    /** \cond INTERNAL */
    /**
     * Provide a string that can be used as a hash for a type.
     */
    template <typename T>
    inline auto
    hash(const std::string &suffix = "") -> std::string
    {
      using underlying_type = std::remove_cv_t<std::remove_reference_t<T>>;
      using stored_type     = std::shared_ptr<underlying_type>;

      std::string alias;
      {
        const std::lock_guard<std::mutex> lock(type_aliases_mutex());
        const auto                        it =
          type_aliases().find(typeid(underlying_type).hash_code());
        if (it != type_aliases().end())
          alias = it->second;
      }

      const auto  resolved_underlying = entt::resolve<underlying_type>();
      std::string base_identifier =
        !alias.empty() ?
          alias :
          (resolved_underlying ?
             type_identifier(resolved_underlying) :
             std::string(entt::type_id<underlying_type>().name()));

      const char *base_name_ptr = store_identifier(base_identifier);
      const char *type_name_ptr = base_name_ptr;
      if (!suffix.empty() && base_identifier.find('(') != std::string::npos)
        type_name_ptr = store_identifier(suffix);
      const auto type_id = entt::hashed_string{type_name_ptr}.value();
      entt::meta_factory<stored_type>().type(type_id, type_name_ptr);

      if (suffix.empty())
        return base_identifier;

      // For function/method types, prefer the supplied suffix (usually a name)
      // over the full signature to keep identifiers readable and stable.
      if (base_identifier.find('(') != std::string::npos)
        {
          store_identifier(suffix);
          return suffix;
        }

      const auto identifier = base_identifier + suffix;
      store_identifier(identifier);
      return identifier;
    }



    inline auto
    hash(const entt::meta_any &obj, const std::string &suffix = "")
      -> std::string
    {
      return type_identifier(obj.type(), suffix);
    }



    /**
     * Allow callers to override the canonical name used for a given type.
     */
    template <typename T>
    inline void
    set_type_alias(const std::string &alias)
    {
      using underlying_type = std::remove_cv_t<std::remove_reference_t<T>>;
      const std::lock_guard<std::mutex> lock(type_aliases_mutex());
      type_aliases()[typeid(underlying_type).hash_code()] = alias;
    }



    inline auto
    hash(const std::shared_ptr<entt::meta_any> &obj,
         const std::string                     &suffix = "") -> std::string
    {
      return (obj && *obj) ? hash(*obj, suffix) : suffix;
    }



    /**
     * Provide a string that can be used as a hash for a value instance.
     */
    template <typename T>
    inline auto
    hash(const T && /*unused*/, const std::string &suffix = "") -> std::string
    {
      return hash<std::remove_cv_t<std::remove_reference_t<T>>>(suffix);
    }



    /** \endcond */
  } // namespace detail

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
    return std::make_shared<NodeObject>(detail::hash<T>());
  }



  /**
   * Construct a pointer to a NodeObject for a named method
   */
  template <typename Arg>
  inline auto
  make_method_node(const std::string &method_name, Arg &&) -> NodeObjectPtr
  {
    std::string hash = "";
    if constexpr (detail::is_callable<Arg>::value)
      {
        // If Arg is callable, hash the std::function type
        using FuncType = decltype(std::function{std::declval<Arg>()});
        hash           = detail::hash<FuncType>(method_name);
      }
    else
      {
        hash = detail::hash<Arg>(method_name);
      }
    return std::make_shared<NodeObject>(hash);
  }

  /**
   * The different types of connections that can be made between nodes.
   */
  enum class ConnectionType : unsigned int
  {

    /**
     * Invalid connection type
     */
    none = 0x000,

    /**
     * Input only
     */
    input = 0x001,

    /**
     * Output only
     */
    output = 0x002,

    /**
     * Input and output
     */
    pass_through = 0x003,

    /**
     * Special connection to indicate that this input/output is the node itself
     */
    self = 0x006,
  };

  /**
   * The different types of nodes.
   */
  enum class NodeType : unsigned int
  {

    /**
     * The node has not been initialized with an object
     */
    none,

    /**
     * This is an abstract type. It will never be instantiated
     */
    abstract,

    /**
     * Trivially copyable and constructible types
     */
    elementary_constructor,

    /**
     * Non trivially copyable, but trivially constructible types
     */
    empty_constructor,

    /**
     * Non trivially copyable, and non trivially constructible types
     */
    constructor,

    /**
     * void member function
     */
    void_method,

    /**
     * void const member function
     */
    void_const_method,

    /**
     * non void member function
     */
    method,

    /**
     * non void const member function
     */
    const_method,

    /**
     * void function
     */
    void_function,

    /**
     * non void function
     */
    function,

    /**
     * Network node
     */
    network,
  };

  namespace detail
  {

    /** \cond INTERNAL */
    /**
     * Store all std::functions needed to build a NodeObject.
     *
     * This type is internal and not part of the public API surface.
     */
    struct NodeObjectInitializer
    {
      /**
       * Human-readable type name (not serialized).
       */
      std::string type_name;

      /**
       * Node type enum.
       */
      NodeType node_type = NodeType::none;

      /**
       * Execution function for this node.
       */
      std::function<std::shared_ptr<
        entt::meta_any>(const NodeObjectPtr &, std::vector<NodeObjectPtr> args)>
        executor =
          [](const NodeObjectPtr &,
             std::vector<NodeObjectPtr>) -> std::shared_ptr<entt::meta_any> {
        return std::make_shared<entt::meta_any>();
      };

      /**
       * Parse a string into the stored value.
       */
      std::function<std::shared_ptr<entt::meta_any>(std::string)> parse_string;

      /**
       * Convert the stored value to a string.
       */
      std::function<std::string(std::shared_ptr<entt::meta_any>)> to_string;

      /**
       * Convert to base class where needed.
       */
      std::function<std::shared_ptr<entt::meta_any>(
        std::shared_ptr<entt::meta_any>)>
        to_base = [](std::shared_ptr<entt::meta_any> a)
        -> std::shared_ptr<entt::meta_any> { return a; };

      /**
       * JSON serialization template for this node type.
       */
      mutable json json_serializer;
    };

    /**
     * Serialized node interface (arguments/inputs/outputs).
     */
    struct NodeInterface
    {
      /**
       * Argument descriptions.
       */
      json arguments = json::array();

      /**
       * Input indices.
       */
      json inputs = json::array();

      /**
       * Output indices.
       */
      json outputs = json::array();
    };

    /** \endcond */
  } // namespace detail

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
  auto
  build_network_interface(const std::shared_ptr<Network> &net)
    -> detail::NodeInterface;

  /**
   * @class NodeObject
   * A class that represents an object of any type.
   *
   * The object itself is stored in a std::shared_ptr<entt::meta_any>. To allow
   * for serialization and to build non trivially constructible classes, the
   * actual type stored in std::shared_ptr<entt::meta_any> is a shared pointer
   * to the object.
   *
   * The object is built only when calling the operator() function. This allows
   * you to connect arguments to this object, in case the building of the object
   * requires other objects.
   *
   * @code
   * coral::NodeObject::register_elementary_type<int>();
   * coral::NodeObjectPtr value = coral::make_node(3);
   * if (value->ready())
   *   (void)(*value)();
   * int v = value->get<int>();
   * @endcode
   */
  class NodeObject : public std::enable_shared_from_this<NodeObject>
  {
  public:
    friend class Network;
    friend std::shared_ptr<entt::meta_any> &
    detail::meta_any_ref(NodeObject &);
    friend const std::shared_ptr<entt::meta_any> &
    detail::meta_any_ref(const NodeObject &);
    NodeObject() = default;

    /**
     * Construct a new object from trivially constructible and copyable types.
     */
    template <typename T>
    NodeObject(const T &data);

    /**
     * Construct NodeObject from a std::shared_ptr<entt::meta_any>. The
     * std::shared_ptr<entt::meta_any> is supposed to contain a shared pointer
     * to a registered type.
     */
    NodeObject(const std::shared_ptr<entt::meta_any> &data);

    /**
     * Construct a new object from a shared pointer to a type.
     */
    template <typename T>
    NodeObject(std::shared_ptr<T> data);

    /**
     * Try to construct a new object from a hash string. If the hash is not
     * found, we store the actual string.
     */
    NodeObject(const std::string &hash_str);

    /**
     * Build a NodeObject from a hash string.
     */
    NodeObject(const char *hash_str);

    /**
     * Return the registry of all types known to this class. If you try to
     * instantiate a class that is not in the registry, an exception will be
     * thrown.
     */
    static auto
    get_registry() -> json;

    static auto
    is_network_type(const std::string &hash_str) -> bool;

    static auto
    build_network_interface(const std::shared_ptr<entt::meta_any> &value)
      -> detail::NodeInterface;

    void
    override_interface(const json &arguments,
                       const json &inputs,
                       const json &outputs);

    auto
    ready() const -> bool;

    void
    parse_string(const std::string &value_str);

    auto
    to_string() const -> std::string;

    /**
     * Run the executor function of the object->
     *
     * @return true If the executor function was run successfully.
     * @return false If the executor function failed to run (i.e.,
     * if it failed when running or if some of the arguments were not ready.)
     */
    auto
    operator()() -> bool;

    /**
     * Set the arguments of the node executor.
     */
    void
    set_arguments(const std::vector<NodeObjectPtr> &args);

    /**
     * Register a new type in the json registry. This method does not add
     * constructors.
     */
    template <typename T>
    static auto
    register_json_header(const std::string &suffix = "")
      -> detail::NodeObjectInitializer &;

    /**
     * Register a new type in the json registry, for types that require
     * additional arguments. This method does not add constructors.
     */
    template <typename T, typename... Args>
    static auto
    register_json_header(const std::vector<std::string> &arg_names,
                         const std::string              &suffix = "")
      -> detail::NodeObjectInitializer &;

    /**
     * Register a non-trivially constructible type T with constructor arguments.
     */
    template <typename T, typename... Args>
    static auto
    register_type(const std::vector<std::string> &arg_names)
      -> detail::NodeObjectInitializer &;

    /**
     * Register an elementary type. This is a type that does not require any
     * arguments to be constructed, it is trivially copyable, and its values
     * can be deduced from a string using
     * dealii::Patterns::Tools::parse_string().
     */
    template <typename T>
    static auto
    register_elementary_type() -> detail::NodeObjectInitializer &
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
        [](const std::string &value) -> std::shared_ptr<entt::meta_any> {
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

        return std::make_shared<entt::meta_any>(t);
      };

      // Add to the initializer the emtpy executor.
      initializer.executor = [](const NodeObjectPtr &,
                                const std::vector<NodeObjectPtr> &args)
        -> std::shared_ptr<entt::meta_any> {
        if (args.size() != 0)
          throw std::runtime_error("Wrong number of arguments.");
        return std::make_shared<entt::meta_any>(std::make_shared<T>());
      };

      // Add value parser
      initializer.to_string =
        [](const std::shared_ptr<entt::meta_any> &value) -> std::string {
        const auto ptr = value->template try_cast<std::shared_ptr<T>>();
        if (ptr == nullptr)
          throw std::runtime_error(
            "Could not cast meta_any to requested shared_ptr type.");
        const T &t = **ptr;

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
    register_type() -> detail::NodeObjectInitializer &
    {
      auto &initializer                        = register_json_header<T>();
      initializer.json_serializer["node_type"] = "empty_constructor";
      initializer.node_type                    = NodeType::empty_constructor;
      initializer.json_serializer["outputs"].push_back(-1);

      // Add to the initializer the emtpy executor.
      initializer.executor = [](const NodeObjectPtr &,
                                const std::vector<NodeObjectPtr> &args)
        -> std::shared_ptr<entt::meta_any> {
        if (args.size() != 0)
          throw std::runtime_error("Wrong number of arguments.");
        return std::make_shared<entt::meta_any>(std::make_shared<T>());
      };
      return initializer;
    }

    /**
     * Register an abstract type. This is a type that will never be
     * constructed.
     */
    template <typename T>
    static auto
    register_abstract_type() -> detail::NodeObjectInitializer &
    {
      auto &initializer                        = register_json_header<T>();
      initializer.json_serializer["node_type"] = "abstract";
      initializer.node_type                    = NodeType::abstract;

      // Add to the initializer the emtpy executor.
      initializer.executor =
        [](const NodeObjectPtr &,
           std::vector<NodeObjectPtr>) -> std::shared_ptr<entt::meta_any> {
        return std::make_shared<entt::meta_any>(std::shared_ptr<T>());
      };
      return initializer;
    }

    /**
     * Register a std::function type. Function types cannot be serialized to
     * JSON, so we register them without value serialization support.
     */
    template <typename R, typename... Args>
    static auto
    register_function_type() -> detail::NodeObjectInitializer &
    {
      using FunctionType = std::function<R(Args...)>;
      auto &initializer = register_json_header<FunctionType>();
      initializer.json_serializer["node_type"] = "empty_constructor";
      initializer.node_type                    = NodeType::empty_constructor;
      initializer.json_serializer["outputs"].push_back(-1);

      // Add executor that creates empty std::function
      initializer.executor = [](const NodeObjectPtr &,
                                const std::vector<NodeObjectPtr> &args)
        -> std::shared_ptr<entt::meta_any> {
        if (args.size() != 0)
          throw std::runtime_error("Wrong number of arguments.");
        return std::make_shared<entt::meta_any>(
          std::make_shared<FunctionType>());
      };

      // Note: parse_string and to_string are intentionally not set
      // because std::function objects cannot be serialized to/from JSON
      return initializer;
    }

    /**
     * Register a non-trivially constructible type T derived from type B.
     */
    template <typename B, typename T>
    static auto
    register_derived_type() -> detail::NodeObjectInitializer &
    {
      auto &initializer = register_type<T>();
      initializer.json_serializer["outputs"].push_back(-1);

      auto &base_initializer = register_abstract_type<B>();
      base_initializer.json_serializer["derived"].push_back(
        initializer.json_serializer["type"]);
      initializer.json_serializer["base"] =
        base_initializer.json_serializer["type"];

      using stored_derived =
        std::shared_ptr<std::remove_cv_t<std::remove_reference_t<T>>>;
      entt::meta_factory<stored_derived>()
        .template conv<&detail::shared_ptr_to_base<B, T>>();

      initializer.to_base = [](std::shared_ptr<entt::meta_any> a)
        -> std::shared_ptr<entt::meta_any> {
        const auto ptr = a->template try_cast<std::shared_ptr<T>>();
        if (ptr == nullptr)
          throw std::runtime_error("Could not cast derived type to base.");
        return std::make_shared<entt::meta_any>(
          std::static_pointer_cast<B>(*ptr));
      };

      return initializer;
    }

    template <typename B, typename T, typename... Args>
    static auto
    register_derived_type(const std::vector<std::string> &arg_names)
      -> detail::NodeObjectInitializer &;

    /**
     * Same as above, for objects that require a single argument.
     */
    template <typename T, typename Arg>
    static auto
    register_type(const std::string &arg_name)
      -> detail::NodeObjectInitializer &
    {
      return register_type<T, Arg>(std::vector<std::string>{{arg_name}});
    }

    /**
     * Same as above, for objects that require a single argument.
     */
    template <typename B, typename T, typename Arg>
    static auto
    register_derived_type(const std::string &arg_name)
      -> detail::NodeObjectInitializer &;

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
          initializer.json_serializer["node_type"] = "void_method";
          initializer.node_type                    = NodeType::void_method;

          // Add the method to the initializer
          initializer.executor =
            [ptr, &initializer, method_name](const NodeObjectPtr &,
                                             std::vector<NodeObjectPtr> args)
            -> std::shared_ptr<entt::meta_any> {
            if (args.size() != 1 + sizeof...(Args))
              throw std::runtime_error("Wrong number of arguments.");
            auto &obj = args[0]->get<T>();
            args.erase(args.begin()); // remove the first element

            auto tuple = detail::cast_args<Args...>(args);
            slog_debug("%s: %s",
                       method_name.c_str(),
                       initializer.type_name.c_str());
            std::apply(
              [&obj, ptr](auto &&...unpackedArgs) {
                (obj.*ptr)(*unpackedArgs...);
              },
              tuple);
            return std::make_shared<entt::meta_any>(
              std::make_shared<ThisMethod>(ptr));
          };
        }
      else
        {
          auto &initializer =
            register_json_header<ThisMethod, T &, ReturnType &, Args...>(
              arg_names, method_name);
          initializer.json_serializer["node_type"] = "method";
          initializer.node_type                    = NodeType::method;
          const bool output_is_elementary =
            is_registered_elementary_type<ReturnType>();
          if (output_is_elementary)
            set_output_only(initializer, 1);

          // Add to the initializer the emtpy executor.
          initializer.executor =
            [ptr, &initializer, method_name, output_is_elementary](
              const NodeObjectPtr &, std::vector<NodeObjectPtr> args)
            -> std::shared_ptr<entt::meta_any> {
            if (args.size() != 2 + sizeof...(Args))
              throw std::runtime_error("Wrong number of arguments.");
            auto &obj = args[0]->get<T>();
            if (output_is_elementary && !args[1]->ready())
              (*args[1])();
            auto &ret = args[1]->get<ReturnType>();

            // Create a copy of args without the first two elements for function
            // execution
            std::vector<NodeObjectPtr> function_args(args.begin() + 2,
                                                     args.end());

            slog_debug("%s: %s",
                       method_name.c_str(),
                       initializer.type_name.c_str());
            auto tuple = detail::cast_args<Args...>(function_args);
            ret        = std::apply(
              [&obj, ptr](auto &&...unpackedArgs) {
                return (obj.*ptr)(*unpackedArgs...);
              },
              tuple);
            return std::make_shared<entt::meta_any>(
              std::make_shared<ThisMethod>(ptr));
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

          // Add the method to the initializer
          initializer.executor =
            [ptr, &initializer, method_name](const NodeObjectPtr &,
                                             std::vector<NodeObjectPtr> args)
            -> std::shared_ptr<entt::meta_any> {
            if (args.size() != 1 + sizeof...(Args))
              throw std::runtime_error("Wrong number of arguments.");
            auto &obj = args[0]->get<T>();
            args.erase(args.begin()); // remove the first element

            auto tuple = detail::cast_args<Args...>(args);
            slog_debug("method: %s", initializer.type_name.c_str());
            std::apply(
              [&obj, ptr](auto &&...unpackedArgs) {
                (obj.*ptr)(*unpackedArgs...);
              },
              tuple);
            return std::make_shared<entt::meta_any>(
              std::make_shared<ThisMethod>(ptr));
          };
        }
      else
        {
          auto &initializer =
            register_json_header<ThisMethod, const T &, ReturnType &, Args...>(
              arg_names, method_name);
          initializer.json_serializer["node_type"] = "const_method";
          initializer.node_type                    = NodeType::const_method;
          const bool output_is_elementary =
            is_registered_elementary_type<ReturnType>();
          if (output_is_elementary)
            set_output_only(initializer, 1);

          // Add to the initializer the emtpy executor.
          initializer.executor =
            [ptr, &initializer, method_name, output_is_elementary](
              const NodeObjectPtr &, std::vector<NodeObjectPtr> args)
            -> std::shared_ptr<entt::meta_any> {
            if (args.size() != 2 + sizeof...(Args))
              throw std::runtime_error("Wrong number of arguments.");
            auto &obj = args[0]->get<T>();
            if (output_is_elementary && !args[1]->ready())
              (*args[1])();
            auto &ret = args[1]->get<ReturnType>();
            args.erase(args.begin()); // remove the class
            args.erase(args.begin()); // remove the return type
            slog_debug("method: %s", initializer.type_name.c_str());
            auto tuple = detail::cast_args<Args...>(args);
            ret        = std::apply(
              [&obj, ptr](auto &&...unpackedArgs) {
                (obj.*ptr)(*unpackedArgs...);
              },
              tuple);
            return std::make_shared<entt::meta_any>(
              std::make_shared<ThisMethod>(ptr));
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
          initializer.json_serializer["node_type"] = "void_function";
          initializer.node_type                    = NodeType::void_function;

          // Add the method to the initializer
          initializer.executor =
            [ptr, &initializer, method_name](const NodeObjectPtr &,
                                             std::vector<NodeObjectPtr> args)
            -> std::shared_ptr<entt::meta_any> {
            if (args.size() != sizeof...(Args))
              throw std::runtime_error("Wrong number of arguments.");

            auto tuple = detail::cast_args<Args...>(args);
            slog_debug("void function: %s [%s]",
                       method_name.c_str(),
                       initializer.type_name.c_str());
            std::apply([ptr](auto &&...unpackedArgs) { ptr(*unpackedArgs...); },
                       tuple);
            return std::make_shared<entt::meta_any>(
              std::make_shared<ThisMethod>(ptr));
          };
        }
      else
        {
          // Treat output sepearately, as it is the first argument
          auto &initializer =
            register_json_header<ThisMethod, ReturnType &, Args...>(
              arg_names, method_name);
          initializer.json_serializer["node_type"] = "function";
          initializer.node_type                    = NodeType::function;
          const bool output_is_elementary =
            is_registered_elementary_type<ReturnType>();
          if (output_is_elementary)
            set_output_only(initializer, 0);

          // Add the method to the initializer
          initializer.executor =
            [ptr, &initializer, method_name, output_is_elementary](
              const NodeObjectPtr &, std::vector<NodeObjectPtr> args)
            -> std::shared_ptr<entt::meta_any> {
            if (args.size() != 1 + sizeof...(Args))
              throw std::runtime_error("Wrong number of arguments.");
            if (output_is_elementary && !args[0]->ready())
              (*args[0])();
            auto &ret = args[0]->get<ReturnType>();
            args.erase(args.begin()); // remove the first element

            auto tuple = detail::cast_args<Args...>(args);
            slog_debug("non-void function: %s [%s]",
                       method_name.c_str(),
                       initializer.type_name.c_str());
            ret = std::apply(
              [ptr](auto &&...unpackedArgs) { return ptr(*unpackedArgs...); },
              tuple);
            return std::make_shared<entt::meta_any>(
              std::make_shared<ThisMethod>(ptr));
          };
        }

      // Phase 2: Automatically register the corresponding std::function type
      // if not already registered
      using FunctionType = std::function<ReturnType(Args...)>;
      const std::string function_type_hash = detail::hash<FunctionType>();

      if (initializers.find(function_type_hash) == initializers.end())
        {
          // Build clean type name for the function type
          std::string clean_name = "std::function<";
          clean_name += boost::core::type_name<ReturnType>();
          clean_name += "(";
          if constexpr (sizeof...(Args) > 0)
            {
              std::vector<std::string> arg_type_names = {
                boost::core::type_name<Args>()...};
              for (size_t i = 0; i < arg_type_names.size(); ++i)
                {
                  if (i > 0)
                    clean_name += ", ";
                  clean_name += arg_type_names[i];
                }
            }
          clean_name += ")>";

          // Set type alias and register
          detail::set_type_alias<FunctionType>(clean_name);
          register_function_type<ReturnType, Args...>();
        }
    }

    // Overload for raw function pointers
    template <typename ReturnType, typename... Args>
    static void
    register_function(ReturnType (*ptr)(Args...),
                      std::vector<std::string> arg_names);

    // Helper to deduce template arguments automatically
    template <typename Lambda>
    static void
    register_function(Lambda func, std::vector<std::string> arg_names);

    /**
     * Get a shared pointer of the stored object->
     *
     * Will throw if the object is not ready, or if the stored object is not
     * of the requested type.
     */
    template <typename T>
    auto
    get_shared() -> std::shared_ptr<T>;

    /**
     * Get a writeable reference to the stored object->
     */
    template <typename T>
    T &
    get();

    /**
     * Get a const reference to the stored object->
     */
    template <typename T>
    const T &
    get() const;

    /**
     * Return a Pointer to the ith output.
     */
    NodeObjectPtr
    get_output(const unsigned int index);

    /**
     * Returns true if the input at @p index is bound to a node.
     */
    bool
    is_input_bound(const unsigned int index) const;

    /**
     * Returns true if the input at @p index is a pass-through input.
     */
    bool
    is_passthrough_input(const unsigned int index) const;

    /**
     * Returns true if the output at @p index is bound to a node.
     */
    bool
    is_output_bound(const unsigned int index) const;

    /**
     * Returns true if the output at @p index can be rebound.
     */
    bool
    is_bindable(const unsigned int index) const;

    /**
     * Bind the ith output to a new NodeObject.
     *
     * @code
     * node->bind_output(0, output_node);
     * @endcode
     */
    void
    bind_output(const unsigned int index, const NodeObjectPtr &value);

    /**
     * Returns true if any inputs are unbound.
     */
    bool
    has_unbound_inputs() const;

    /**
     * Returns true if any outputs are unbound.
     */
    bool
    has_unbound_outputs() const;

    /**
     * Return a Pointer to the @p index -th input.
     */
    NodeObjectPtr
    get_input(const unsigned int index);

    /**
     * Bind an input slot to a NodeObject.
     *
     * @code
     * node->bind_input(0, other);
     * @endcode
     */
    void
    bind_input(const unsigned int index, const NodeObjectPtr &value);

    /**
     * Get a shared pointer to the underlying object (const).
     */
    template <typename T>
    std::shared_ptr<const T>
    get_shared() const;

    /**
     * Assign from a shared pointer to a registered type.
     */
    template <typename T>
    NodeObject &
    operator=(std::shared_ptr<T> data);

    /**
     * Assign from a value of a registered type.
     */
    template <typename T>
    NodeObject &
    operator=(const T &data);

    /**
     * Return the JSON info record for this node.
     */
    const json &
    get_info() const;

    /**
     * Get the hash of the stored object->
     */
    std::string
    hash() const;

    /**
     * Return the human-readable type name for this node.
     */
    std::string
    type_name() const;

    /**
     * Return the node type enum.
     */
    NodeType
    node_type() const;

    /**
     * Get the number of arguments that this node has.
     */
    size_t
    n_arguments() const;

    /**
     * Get the number of inputs.
     */
    size_t
    n_inputs() const;

    /**
     * Get the number of outputs.
     */
    size_t
    n_outputs() const;

  private:
    /**
     * Return true if an output index maps to a pass-through argument.
     */
    bool
    is_passthrough_output(const unsigned int index) const;

    /**
     * Find the input index that maps to a given argument index.
     */
    unsigned int
    input_index_for_argument(const int argument_index) const;

    /**
     * The actual object is stored here as a std::shared_ptr<entt::meta_any>.
     */
    std::shared_ptr<entt::meta_any> object;

    /**
     * Anything required to build a std::shared_ptr<T> object, and to
     * manipulate it.
     */
    detail::NodeObjectInitializer initializer;

    /**
     * The arguments to pass to the executor.
     */
    std::vector<NodeObjectPtr> arguments;

    /**
     * Arguments to connection type.
     */
    std::vector<ConnectionType> arguments_types;

    /**
     * Track whether inputs and outputs have been explicitly bound.
     */
    std::vector<bool> input_bound;
    std::vector<bool> output_bound;

    /**
     * A list of all known types and their initializers.
     */
    // Thread-safety: registration is not synchronized; treat as
    // single-threaded init before concurrent use.
    static inline std::map<std::string, detail::NodeObjectInitializer>
      initializers;

    /**
     * Hash string that identifies the Network node type.
     */
    static inline std::string network_type_hash;

    /**
     * Builder for network interfaces from stored values.
     */
    static inline std::function<detail::NodeInterface(
      const std::shared_ptr<entt::meta_any> &)>
      network_interface_builder;

    template <typename T>
    static bool
    is_registered_elementary_type();

    /**
     * Mark an argument as output-only in the JSON metadata.
     */
    static void
    set_output_only(detail::NodeObjectInitializer &initializer,
                    unsigned int                   arg_index);

    /**
     * Input indices mapping to arguments.
     */
    std::vector<int> input_indices;

    /**
     * Output indices mapping to arguments. If the content is -1, then we
     * return the object itself.
     */
    std::vector<int> output_indices;

    /**
     * Initialize input index mappings from JSON metadata.
     */
    void
    initialize_inputs();

    /**
     * Initialize output index mappings from JSON metadata.
     */
    void
    initialize_outputs();

    /**
     * Initialize argument list and connection types from JSON metadata.
     */
    void
    initialize_arguments();

    /**
     * Configure network-specific interface overrides.
     */
    void
    setup_network_if_needed();

  public:
    /**
     * Bind all inputs using a list of node/output pairs.
     *
     * @code
     * node->bind_inputs({{upstream, 0}});
     * @endcode
     */
    void
    bind_inputs(
      const std::vector<std::pair<NodeObjectPtr, unsigned int>> &inputs);
  };

  // Template implementations kept in the main header for visibility.
  template <typename T>
  inline NodeObject::NodeObject(const T &data)
    : NodeObject(std::make_shared<T>(data))
  {}

  template <typename T>
  inline NodeObject::NodeObject(std::shared_ptr<T> data)
  {
    object        = std::make_shared<entt::meta_any>(data);
    auto hash_str = detail::hash<T>();
    try
      {
        initializer = initializers.at(hash_str);
      }
    catch (const std::out_of_range &)
      {
        throw std::runtime_error(
          "Type " + std::string(typeid(T).name()) +
          " is not registered. Before using it, you should call "
          "one of the NodeObject::register_*<" +
          std::string(typeid(T).name()) + ">(...) functions.");
      }
    initialize_arguments();
    initialize_inputs();
    initialize_outputs();
    setup_network_if_needed();
  }



  template <typename T>
  inline auto
  NodeObject::register_json_header(const std::string &suffix)
    -> detail::NodeObjectInitializer &
  {
    auto hash_str = detail::hash<T>(suffix);
    if (initializers.find(hash_str) != initializers.end())
      // Reset the initializer
      initializers[hash_str] = {};

    auto &initializer                        = initializers[hash_str];
    initializer.type_name                    = boost::core::type_name<T>();
    initializer.json_serializer["type"]      = hash_str;
    initializer.json_serializer["arguments"] = json::array();
    initializer.json_serializer["inputs"]    = json::array();
    initializer.json_serializer["outputs"]   = json::array();
    return initializer;
  }



  template <typename T, typename... Args>
  inline auto
  NodeObject::register_json_header(const std::vector<std::string> &arg_names,
                                   const std::string              &suffix)
    -> detail::NodeObjectInitializer &
  {
    auto &initializer = register_json_header<T>(suffix);

    // Now take care of the arguments.
    std::vector<NodeObjectPtr> args = {std::make_shared<NodeObject>(
      std::shared_ptr<std::remove_cv_t<std::remove_reference_t<Args>>>())...};

    std::vector<ConnectionType> arg_connection_types = {
      connection_type<Args>()...};

    if (args.size() != arg_names.size())
      throw std::runtime_error("Wrong number of arguments.");

    for (unsigned int i = 0; i < args.size(); ++i)
      {
        initializer.json_serializer["arguments"][i]["name"] = arg_names[i];
        initializer.json_serializer["arguments"][i]["type"] = args[i]->hash();
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
   * json serialization of a Nodeobject->
   */
  void
  to_json(json &j, const NodeObjectPtr &obj);

  /**
   * json deserialization of a Nodeobject->
   */
  void
  from_json(const json &j, NodeObjectPtr &obj);

  namespace detail
  {

    /** \cond INTERNAL */
    /**
     * Implementation of cast_args.
     */
    template <typename... Args, std::size_t... Is>
    inline std::tuple<
      std::shared_ptr<std::remove_const_t<std::remove_reference_t<Args>>>...>
    cast_args_impl(const std::vector<NodeObjectPtr> &args,
                   std::index_sequence<Is...>)
    {
      try
        {
          return std::make_tuple(
            args[Is]
              ->get_shared<
                std::remove_const_t<std::remove_reference_t<Args>>>()...);
        }
      catch (const std::exception &e)
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
    cast_args(const std::vector<NodeObjectPtr> &args)
    {
      return cast_args_impl<Args...>(args, std::index_sequence_for<Args...>{});
    }

    /** \endcond */
  } // namespace detail

  /**
   * Connect the inputs of a node.
   *
   * This function sets the inputs of @p node to the output connectors of
   * nodes in @p inputs, in particular, it sets the input of the node to
   * @p inputs[i].first->outputs[inputs[i].second].
   *
   * @param node The node to connect
   * @param inputs The pair input/output connector of the node
   *
   * @code
   * coral::connect(node, {{upstream, 0}});
   * @endcode
   */
  void
  connect(
    NodeObjectPtr                                             &node,
    const std::vector<std::pair<NodeObjectPtr, unsigned int>> &inputs = {});

  template <typename T, typename... Args>
  inline auto
  NodeObject::register_type(const std::vector<std::string> &arg_names)
    -> detail::NodeObjectInitializer &
  {
    auto &initializer = register_json_header<T, Args...>(arg_names);
    initializer.json_serializer["node_type"] = "constructor";
    initializer.node_type                    = NodeType::constructor;
    initializer.json_serializer["outputs"].push_back(-1);

    // And the executor.
    initializer.executor = [&initializer](const NodeObjectPtr &,
                                          std::vector<NodeObjectPtr> args)
      -> std::shared_ptr<entt::meta_any> {
      if (args.size() != sizeof...(Args))
        throw std::runtime_error("Wrong number of arguments.");
      auto tuple = detail::cast_args<Args...>(args);
      slog_debug("constructor: %s", initializer.type_name.c_str());
      return std::apply(
        [](auto &&...unpackedArgs) {
          return std::make_shared<entt::meta_any>(
            std::make_shared<T>(*unpackedArgs...));
        },
        tuple);
    };
    return initializer;
  }



  template <typename B, typename T, typename... Args>
  inline auto
  NodeObject::register_derived_type(const std::vector<std::string> &arg_names)
    -> detail::NodeObjectInitializer &
  {
    auto &initializer      = register_type<T, Args...>(arg_names);
    auto &base_initializer = register_abstract_type<B>();

    base_initializer.json_serializer["derived"].push_back(
      initializer.json_serializer["type"]);

    initializer.json_serializer["base"] =
      base_initializer.json_serializer["type"];

    // Register entt conversion shared_ptr<Derived> -> shared_ptr<Base>
    using stored_derived =
      std::shared_ptr<std::remove_cv_t<std::remove_reference_t<T>>>;
    entt::meta_factory<stored_derived>()
      .template conv<&detail::shared_ptr_to_base<B, T>>();

    // Add the conversion to the base class.
    initializer.to_base =
      [](std::shared_ptr<entt::meta_any> a) -> std::shared_ptr<entt::meta_any> {
      const auto ptr = a->template try_cast<std::shared_ptr<T>>();
      if (ptr == nullptr)
        throw std::runtime_error("Could not cast derived type to base.");
      return std::make_shared<entt::meta_any>(
        std::static_pointer_cast<B>(*ptr));
    };
    return initializer;
  }



  template <typename B, typename T, typename Arg>
  inline auto
  NodeObject::register_derived_type(const std::string &arg_name)
    -> detail::NodeObjectInitializer &
  {
    return register_derived_type<B, T, Arg>(
      std::vector<std::string>{{arg_name}});
  }


  template <typename ReturnType, typename... Args>
  inline void
  NodeObject::register_function(ReturnType (*ptr)(Args...),
                                std::vector<std::string> arg_names)
  {
    std::function<ReturnType(Args...)> func = ptr;
    register_function(func, arg_names);
  }



  template <typename Lambda>
  inline void
  NodeObject::register_function(Lambda func, std::vector<std::string> arg_names)
  {
    auto f = std::function{func};
    register_function(f, arg_names);
  }



  template <typename T>
  inline auto
  NodeObject::get_shared() -> std::shared_ptr<T>
  {
    if (!ready())
      throw std::runtime_error("Object is not ready.");
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    std::shared_ptr<type> ptr;

    if (hash() != detail::hash<type>())
      {
        auto &j = initializer.json_serializer;
        if (!(j.contains("base") &&
              (detail::hash<type>() == j.at("base").get<std::string>())))
          throw std::runtime_error("Cannot cast object of type " + type_name() +
                                   " to object of type " +
                                   boost::core::type_name<type>() + ".");
        auto new_object = initializer.to_base(object);
        if (!(new_object && *new_object))
          throw std::runtime_error("New object does not have value.");
        if (!(detail::hash(new_object) == j.at("base").get<std::string>()))
          throw std::runtime_error("New object does not have the right hash.");
        const auto cast_ptr =
          new_object->template try_cast<std::shared_ptr<type>>();
        if (cast_ptr == nullptr)
          throw std::runtime_error("Could not cast converted object to " +
                                   std::string(boost::core::type_name<type>()));
        ptr = *cast_ptr;
      }
    else
      {
        const auto cast_ptr =
          object->template try_cast<std::shared_ptr<type>>();
        if (cast_ptr == nullptr)
          throw std::runtime_error(
            "Could not cast object to shared pointer of type " +
            std::string(boost::core::type_name<type>()) +
            " from object of type " + boost::core::demangle([&]() {
              const char *named = object->type().name();
              if (named != nullptr && named[0] != '\0')
                return named;
              const auto info_name = object->type().info().name();
              static thread_local std::string info_name_str;
              info_name_str = std::string(info_name);
              return info_name_str.c_str();
            }()) +
            ".");
        ptr = *cast_ptr;
      }
    return ptr;
  }



  template <typename T>
  inline T &
  NodeObject::get()
  {
    if (!ready())
      throw std::runtime_error("Object is not ready.");
    return *(get_shared<T>());
  }



  template <typename T>
  inline const T &
  NodeObject::get() const
  {
    if (!ready())
      throw std::runtime_error("Object is not ready.");
    return *(get_shared<T>());
  }



  template <typename T>
  inline std::shared_ptr<const T>
  NodeObject::get_shared() const
  {
    if (!ready())
      throw std::runtime_error("Object is not ready.");
    using type = std::remove_cv_t<std::remove_reference_t<T>>;
    std::shared_ptr<const type> ptr;
    if (hash() != detail::hash<type>())
      {
        auto &j = initializer.json_serializer;
        if (!(j.contains("base") &&
              (detail::hash<type>() == j.at("base").get<std::string>())))
          throw std::runtime_error("Cannot cast object of type " + type_name() +
                                   " to object of type " +
                                   boost::core::type_name<type>() + ".");
        auto new_object = initializer.to_base(object);
        if (!(new_object && *new_object))
          throw std::runtime_error("New object does not have value.");
        if (!(detail::hash(new_object) == j.at("base").get<std::string>()))
          throw std::runtime_error("New object does not have the right hash.");
        const auto cast_ptr =
          new_object->template try_cast<std::shared_ptr<const type>>();
        if (cast_ptr == nullptr)
          throw std::runtime_error("Could not cast converted object to " +
                                   std::string(boost::core::type_name<type>()));
        ptr = *cast_ptr;
      }
    else
      {
        const auto cast_ptr =
          object->template try_cast<std::shared_ptr<const type>>();
        if (cast_ptr == nullptr)
          throw std::runtime_error(
            "Could not cast object to shared pointer of type " +
            std::string(boost::core::type_name<type>()) +
            " from object of type " + boost::core::demangle([&]() {
              const char *named = object->type().name();
              if (named != nullptr && named[0] != '\0')
                return named;
              const auto info_name = object->type().info().name();
              static thread_local std::string info_name_str;
              info_name_str = std::string(info_name);
              return info_name_str.c_str();
            }()) +
            ".");
        ptr = *cast_ptr;
      }
    return ptr;
  }



  template <typename T>
  inline NodeObject &
  NodeObject::operator=(std::shared_ptr<T> data)
  {
    if (!(object && *object))
      {
        *this = NodeObject(data);
        return *this;
      }
    // Check that we store a compatible type first.
    // Check that detail::hash<T>() is contained in hash().
    const auto my_hash = hash();
    const auto type_id = detail::hash<T>();
    if (my_hash.find(type_id) != 0)
      throw std::runtime_error(
        "Object type does not match. My hash is " + my_hash +
        " and the object hash is " + type_id +
        ". They should at least start with the same characters.");
    *this = NodeObject(data);
    return *this;
  }



  template <typename T>
  inline NodeObject &
  NodeObject::operator=(const T &data)
  {
    *this = NodeObject(std::make_shared<T>(data));
    return *this;
  }



  template <typename T>
  inline bool
  NodeObject::is_registered_elementary_type()
  {
    using PlainType     = std::remove_cv_t<std::remove_reference_t<T>>;
    const auto hash_str = detail::hash<PlainType>();
    const auto it       = initializers.find(hash_str);
    return (it != initializers.end()) &&
           (it->second.node_type == NodeType::elementary_constructor);
  }

  namespace detail
  {
    std::shared_ptr<entt::meta_any> &
    meta_any_ref(NodeObject &node);

    const std::shared_ptr<entt::meta_any> &
    meta_any_ref(const NodeObject &node);
  } // namespace detail
} // namespace coral
#if defined(CORAL_HEADER_ONLY) && CORAL_HEADER_ONLY
#  include "coral_implementation.h"
#endif
#endif
