#ifndef CORAL_H
#define CORAL_H

#include <boost/core/type_name.hpp>

#include <any>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <typeindex>
#include <typeinfo>
#include <vector>

#include "json/json.hpp"         // JSON library
#include "taskflow/taskflow.hpp" // Taskflow library

using json = nlohmann::json;

namespace coral
{
  // forward declarations
  class NodeObject;

  /**
   * Provide a string that can be used as a hash for a type.
   */
  std::string
  hash(const std::type_info &type)
  {
    std::stringstream ss;
    ss << std::hex << std::type_index(type).hash_code();
    return ss.str();
  }

  /**
   * Provide a string that can be used as a hash for a type.
   */
  template <typename T>
  std::string
  hash()
  {
    std::shared_ptr<T> ptr;
    return hash(typeid(ptr));
  }


  /**
   * Store all std::functions that need to be used to build a NodeObject, and
   * the corresponding json serialization.
   */
  struct NodeObjectInitializer
  {
    /**
     * Build a new NodeObject, given a list of NodeObject elements as arguments
     * to the executor.
     */
    std::function<std::any(std::vector<NodeObject> args)> executor =
      [](std::vector<NodeObject>) -> std::any { return std::any(); };

    /**
     * The json serialization of the object.
     */
    json json_serializer;
  };

  /**
   * @class NodeObject
   * @brief A class that represents an object of any type.
   *
   * The object itself is stored in a std::any. To allow for serialization and
   * to build non trivially constructible classes, the actual type stored in
   * std::any is a shared pointer to the object.
   *
   * The object is built only when calling the operator() function. This allows
   * you to connect arguments to this object, in case the building of the object
   * requires other objects.
   */
  class NodeObject
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
     * Construct a new object from a shared pointer to a type.
     */
    template <typename T>
    NodeObject(std::shared_ptr<T> data)
    {
      object        = data;
      auto hash_str = coral::hash<T>();
      initializer   = initializers.at(hash_str);
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
          // If we did not find the hash, we treat the string as a type name.
          *this = NodeObject(std::make_shared<std::string>(hash_str));
        }
    }

    NodeObject(const char *hash_str)
      : NodeObject(std::string(hash_str))
    {}


    /**
     * Return the registry of all types known to this class. If you try to
     * instantiate a class that is not in the registry, an exception will be
     * thrown.
     */
    static json
    get_registry()
    {
      json registry;
      for (const auto &[hash_str, initializer] : NodeObject::initializers)
        {
          registry[hash_str] = initializer.json_serializer;
        }
      return registry;
    }

    bool
    ready() const
    {
      return object.has_value();
    }

    /**
     * Run the executor function of the object.
     *
     * @return true If the executor function was run successfully.
     * @return false If the executor function failed to run (i.e.,
     * if it failed when running or if some of the arguments were not ready.)
     */
    bool
    operator()()
    {
      bool is_ready = true;
      for (const auto &arg : arguments)
        if (!arg.ready())
          {
            is_ready = false;
            break;
          }

      AssertThrow(is_ready,
                  dealii::ExcMessage(
                    "Arguments are not ready. You can only call "
                    "this function after all arguments are ready."));
      try
        {
          object = initializer.executor(arguments);
          return object.has_value();
        }
      catch (...)
        {}
      return false;
    }

    /**
     * Set the arguments of this node to other nodes.
     */
    void
    set_args(const std::vector<NodeObject> &args)
    {
      AssertThrow(args.size() == initializer.json_serializer["args"].size(),
                  dealii::ExcMessage(
                    "Wrong number of arguments: " +
                    std::to_string(args.size()) + " instead of " +
                    std::to_string(initializer.json_serializer["args"].size()) +
                    "."));
      this->arguments = args;
    }

    /**
     * Register a new type in the json registry. This method does not add
     * constructors.
     */
    template <typename T>
    static NodeObjectInitializer &
    register_json_header()
    {
      auto hash_str = coral::hash<T>();
      if (initializers.find(hash_str) != initializers.end())
        {
          AssertThrow(false,
                      dealii::ExcMessage("NodeObject with type name \"" +
                                         boost::core::type_name<T>() +
                                         "\" is already registered."));
        }
      auto &initializer                        = initializers[hash_str];
      initializer.json_serializer["type"]      = boost::core::type_name<T>();
      initializer.json_serializer["type_hash"] = hash_str;
      initializer.json_serializer["args"]      = json::object();
      return initializer;
    }

    /**
     * Register a new type in the json registry, for types that require
     * additional arguments. This method does not add constructors.
     */
    template <typename T, typename... Args>
    static NodeObjectInitializer &
    register_json_header(const std::vector<std::string> &arg_names)
    {
      auto &initializer = register_json_header<T>();

      // Now take care of the arguments.
      std::vector<NodeObject> args = {NodeObject(std::shared_ptr<Args>())...};

      AssertThrow(args.size() == arg_names.size(),
                  dealii::ExcMessage("Wrong number of arguments."));

      for (unsigned int i = 0; i < args.size(); ++i)
        {
          initializer.json_serializer["args"][arg_names[i]]["type"] =
            args[i].type_name();
          initializer.json_serializer["args"][arg_names[i]]["type_hash"] =
            args[i].hash();
        }
      return initializer;
    }

    /**
     * Register an elementary type. This is a type that does not require any
     * arguments to be constructed, it is trivially copyable, and its values can
     * be deduced from a string using dealii::Patterns::Tools::to_value().
     */
    template <typename T>
    static void
    register_elementary_type()
    {
      auto &initializer                       = register_json_header<T>();
      initializer.json_serializer["run_type"] = "elementary_constructor";

      // Add to the initializer the emtpy executor.
      initializer.executor = [](std::vector<NodeObject> args) -> std::any {
        AssertThrow(args.size() == 0,
                    dealii::ExcMessage("Wrong number of arguments."));
        return std::any(std::make_shared<T>());
      };
    }

    /**
     * Register a trivially constructible type. This is a type that does not
     * require any arguments to be constructed, but it is not trivially
     * copyable. This is the case for example of dealii::Triangulation<2>.
     */
    template <typename T>
    static void
    register_type()
    {
      auto &initializer                       = register_json_header<T>();
      initializer.json_serializer["run_type"] = "empty_constructor";

      // Add to the initializer the emtpy executor.
      initializer.executor = [](std::vector<NodeObject> args) -> std::any {
        AssertThrow(args.size() == 0,
                    dealii::ExcMessage("Wrong number of arguments."));
        return std::any(std::make_shared<T>());
      };
    }

    /**
     * Register a non-trivially constructible type. This is a type that does
     * require arguments to be constructed, and that it is not trivially
     * copyable. This is the case for example of dealii::FE_Q<2>(), that
     * requires the degree of the finite element to be instantiated.
     */
    template <typename T, typename... Args>
    static void
    register_type(const std::vector<std::string> &arg_names)
    {
      auto &initializer = register_json_header<T, Args...>(arg_names);
      initializer.json_serializer["run_type"] = "constructor";

      // And the executor.
      initializer.executor = [](std::vector<NodeObject> args) -> std::any {
        return std::any(
          std::make_shared<T>(*std::any_cast<std::shared_ptr<Args>>(args)...));
      };
    }

    /**
     * Same as above, for objects that require a single argument.
     */
    template <typename T, typename Arg>
    static void
    register_type(const std::string &arg_name)
    {
      register_type<T, Arg>(std::vector<std::string>{{arg_name}});
    }

    /**
     * A general pointer-to-member-function type.
     */
    template <typename T, typename ReturnType, typename... Args>
    using MethodPtr = ReturnType (T::*)(Args...);

    /**
     * Register a method of a class. This node will have as arguments the
     * object of the class, possibly the output argument, and the arguments of
     * the method.
     *
     * The method can be void or return a value, and can have any number of
     * arguments.
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
    register_method(MethodPtr<T, ReturnType, Args...> ptr,
                    std::vector<std::string>          arg_names)
    {
      using ThisMethod              = MethodPtr<T, ReturnType, Args...>;
      constexpr bool return_is_void = std::is_same_v<ReturnType, void>;
      auto           method_name    = arg_names[0];
      arg_names.erase(arg_names.begin());

      if constexpr (return_is_void)
        {
          auto &initializer = register_json_header<
            ThisMethod,
            T,
            typename std::remove_cv<
              typename std::remove_reference<Args>::type>::type...>(arg_names);
          initializer.json_serializer["run_type"]    = "void_method";
          initializer.json_serializer["method_name"] = method_name;


          // Add to the initializer the emtpy executor.
          initializer.executor =
            [ptr](std::vector<NodeObject> args) -> std::any {
            AssertThrow(args.size() == 1 + sizeof...(Args),
                        dealii::ExcMessage("Wrong number of arguments."));
            auto &obj = args[0].template get<T>();
            args.erase(args.begin()); // remove the first element
            // execute the method. This is a void function.
            (obj.*ptr)(
              *std::any_cast<std::shared_ptr<typename std::remove_cv<
                typename std::remove_reference<Args>::type>::type>>(args)...);
            std::cout << "Executing method " << coral::hash(typeid(ptr))
                      << std::endl;
            return std::any(ptr);
          };
        }
      else
        {
          auto &initializer = register_json_header<
            ThisMethod,
            T,
            ReturnType,
            typename std::remove_cv<
              typename std::remove_reference<Args>::type>::type...>(arg_names);
          initializer.json_serializer["run_type"]    = "method";
          initializer.json_serializer["method_name"] = method_name;
          // Add to the initializer the emtpy executor.
          initializer.executor =
            [ptr](std::vector<NodeObject> args) -> std::any {
            AssertThrow(args.size() == 2 + sizeof...(Args),
                        dealii::ExcMessage("Wrong number of arguments."));
            auto &obj = args[0].template get<T>();
            auto &ret = args[1].template get<ReturnType>();
            args.erase(args.begin()); // remove the class
            args.erase(args.begin()); // remove the return type
            // execute the method. This is a void function.
            ret = (obj.*ptr)(
              *std::any_cast<std::shared_ptr<typename std::remove_cv<
                typename std::remove_reference<Args>::type>::type>>(args)...);
            std::cout << "Executing method " << coral::hash(typeid(ptr))
                      << std::endl;
            return std::any(ptr);
          };
        }
    }

    /**
     * Get a shared pointer of the stored object.
     *
     * Will throw if the object is not ready, or if the stored object is not of
     * the requested type.
     */
    template <typename T>
    std::shared_ptr<T>
    get_shared()
    {
      AssertThrow(ready(), dealii::ExcMessage("Object is not ready."));
      std::shared_ptr<T> ptr;
      try
        {
          ptr = std::any_cast<std::shared_ptr<T>>(object);
        }
      catch (...)
        {
          AssertThrow(false,
                      dealii::ExcMessage(
                        "Cannot cast object to shared pointer of type " +
                        boost::core::type_name<T>() + " from object of type " +
                        boost::core::demangle(object.type().name()) + "."));
        }
      return ptr;
    }

    /**
     * Get a writeable reference to the stored object.
     */
    template <typename T>
    T &
    get()
    {
      AssertThrow(ready(), dealii::ExcMessage("Object is not ready."));
      return *(get_shared<T>());
    }

    /**
     * Get a const reference to the stored object.
     */
    template <typename T>
    const T &
    get() const
    {
      AssertThrow(ready(), dealii::ExcMessage("Object is not ready."));
      return *(get_shared<T>());
    }

    /**
     * Expose the underlying std::any object.
     */
    operator const std::any &() const
    {
      return object;
    }

    /**
     * Expose the underlying std::any object.
     */
    operator std::any &()
    {
      return object;
    }

    template <typename T>
    std::shared_ptr<const T>
    get_shared() const
    {
      std::shared_ptr<const T> ptr;
      try
        {
          ptr = std::any_cast<std::shared_ptr<const T>>(object);
        }
      catch (const std::bad_any_cast &e)
        {
          std::cout << "Cannot cast object to shared pointer of type "
                    << boost::core::type_name<const T>() << std::endl;
          throw;
        }
      return ptr;
    }


    template <typename T>
    NodeObject &
    operator=(std::shared_ptr<T> data)
    {
      if (!object.has_value())
        {
          *this = NodeObject(data);
          return *this;
        }
      else
        {
          // Check that we store a compatible type first.
          AssertThrow(coral::hash<T>() == hash(),
                      dealii::ExcMessage("Object type does not match."));
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
      return initializer.json_serializer;
    }

    /**
     * Get the hash of the stored object.
     */
    std::string
    hash() const
    {
      if (object.has_value())
        {
          // The object is initialized. Check if this is consistent with the
          // initializer, and return its hash.
          const auto object_hash = coral::hash(object.type());
          const auto stored_hash =
            std::string(initializer.json_serializer.at("type_hash"));
          AssertThrow(object_hash == stored_hash,
                      dealii::ExcMessage(
                        "Object type does not match: we should store " +
                        stored_hash + " instead of " + object_hash + "."));
          return object_hash;
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

  private:
    /**
     * The actual object is stored here as a std::any containing a
     * std::shared_ptr<T>.
     */
    std::any object;

    /**
     * Anything required to build a std::shared_ptr<T> object, and to manipulate
     * it.
     */
    NodeObjectInitializer initializer;

    /**
     * The arguments of this node to other nodes.
     */
    std::vector<NodeObject> arguments;

    /**
     * A list of all known types and their initializers.
     */
    static std::map<std::string, NodeObjectInitializer> initializers;
  };

  std::map<std::string, NodeObjectInitializer> NodeObject::initializers;


  /**
   * json serialization of a NodeObject.
   */
  void
  to_json(json &j, const NodeObject &obj)
  {
    j = obj.get_info();
  }

  /**
   * json deserialization of a NodeObject.
   */
  void
  from_json(const json &j, NodeObject &obj)
  {
    obj = NodeObject(j.at("type_hash").template get<std::string>());
  }
} // namespace coral
#endif