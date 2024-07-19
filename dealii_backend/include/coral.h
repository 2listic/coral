#ifndef CORAL_H
#define CORAL_H

#include <deal.II/base/mutable_bind.h>
#include <deal.II/base/patterns.h>

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
#include "type_name.h"

using json = nlohmann::json;

namespace coral
{
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
  inline std::string
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
  inline std::string
  hash()
  {
    std::shared_ptr<std::remove_cv_t<std::remove_reference_t<T>>> ptr;
    return hash(typeid(ptr));
  }

  /**
   * Provide a string that can be used as a hash for a type.
   */
  inline std::string
  hash(const std::any &obj)
  {
    return hash(obj.type());
  }

  /**
   * Provide a string that can be used as a hash for a type.
   */
  template <typename T>
  inline std::string
  hash(const T && /*unused*/)
  {
    std::shared_ptr<std::remove_cv_t<std::remove_reference_t<T>>> ptr;
    return hash(typeid(ptr));
  }

  template <typename... Args>
  NodeObjectPtr
  make_node(Args &&...args)
  {
    return std::make_shared<NodeObject>(args...);
  }

  template <typename T>
  NodeObjectPtr
  make_node()
  {
    return std::make_shared<NodeObject>(hash<T>());
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
    std::function<std::any(std::vector<std::shared_ptr<NodeObject>> args)>
      executor = [](std::vector<std::shared_ptr<NodeObject>>) -> std::any {
      return std::any();
    };

    /**
     * For supported types, we can also parse a string to a value.
     */
    std::function<std::any(std::string)> parse_string;

    /**
     * For supported types, we can also output the value as a string.
     */
    std::function<std::string(std::any)> to_string;

    /**
     * For derived types, we give a way to convert to the base class.
     */
    std::function<std::any(std::any)> to_base = [](std::any a) -> std::any {
      return a;
    };

    /**
     * The json serialization of the object.
     */
    mutable json json_serializer;
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

    NodeObject(const std::any &data)
      : NodeObject(coral::hash(data))
    {}

    /**
     * Construct a new object from a shared pointer to a type.
     */
    template <typename T>
    NodeObject(std::shared_ptr<T> data)
    {
      object        = data;
      auto hash_str = coral::hash<T>();
      try
        {
          initializer = initializers.at(hash_str);
        }
      catch (const std::out_of_range &e)
        {
          AssertThrow(false,
                      dealii::ExcMessage(
                        "Type " + boost::core::type_name<T>() +
                        " is not registered. Before using it, you should call "
                        "one of the NodeObject::register_*<" +
                        boost::core::type_name<T>() + ">(...) functions."));
        }
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

    void
    parse_string(const std::string &value_str)
    {
      if (initializer.parse_string)
        object = initializer.parse_string(value_str);
      else
        {
          AssertThrow(false,
                      dealii::ExcMessage(
                        "No value parser available for this type."));
        }
    }

    std::string
    to_string() const
    {
      if (initializer.to_string)
        {
          AssertThrow(
            ready(),
            dealii::ExcMessage(
              "Object is not ready. You cannot ask for its value yet."));
          return initializer.to_string(object);
        }
      else
        {
          AssertThrow(false,
                      dealii::ExcMessage(
                        "No value parser available for this type."));
        }
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
        if (!arg->ready())
          {
            is_ready = false;
            break;
          }

      AssertThrow(is_ready,
                  dealii::ExcMessage(
                    "Arguments are not ready. You can only call "
                    "this function after all arguments are ready."));
      // try
      //   {
      object = initializer.executor(arguments);
      return object.has_value();
      //   }
      // catch (...)
      //   {
      //     std::cout << "Error in running executor." << std::endl;
      //   }
      // return false;
    }

    /**
     * Set the arguments of this node to other nodes.
     */
    void
    set_args(const std::vector<std::shared_ptr<NodeObject>> &args)
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
        // Reset the initializer
        initializers[hash_str] = {};

      auto &initializer                        = initializers[hash_str];
      initializer.json_serializer["type"]      = boost::core::type_name<T>();
      initializer.json_serializer["type_hash"] = hash_str;
      initializer.json_serializer["args"]      = json::array();
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
      std::vector<std::shared_ptr<NodeObject>> args = {
        std::make_shared<NodeObject>(std::shared_ptr<Args>())...};

      AssertThrow(args.size() == arg_names.size(),
                  dealii::ExcMessage("Wrong number of arguments."));

      for (unsigned int i = 0; i < args.size(); ++i)
        {
          initializer.json_serializer["args"][i]["name"] = arg_names[i];
          initializer.json_serializer["args"][i]["type"] = args[i]->type_name();
          initializer.json_serializer["args"][i]["type_hash"] = args[i]->hash();
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
    static NodeObjectInitializer &
    register_elementary_type()
    {
      auto &initializer                       = register_json_header<T>();
      initializer.json_serializer["run_type"] = "elementary_constructor";
      initializer.json_serializer["value"] =
        dealii::Patterns::Tools::to_string(T());

      // Add to the initializer the emtpy executor.
      initializer.executor =
        [](std::vector<std::shared_ptr<NodeObject>> args) -> std::any {
        AssertThrow(args.size() == 0,
                    dealii::ExcMessage("Wrong number of arguments."));
        return std::any(std::make_shared<T>());
      };

      // Add value parser
      initializer.parse_string = [](std::string value) -> std::any {
        auto t = std::make_shared<T>();
        dealii::Patterns::Tools::to_value(value, *t);
        return std::any(t);
      };


      // Add value parser
      initializer.to_string = [](std::any value) -> std::string {
        const T &t = *std::any_cast<std::shared_ptr<T>>(value);
        return dealii::Patterns::Tools::to_string(t);
      };
      return initializer;
    }

    /**
     * Register a trivially constructible type. This is a type that does not
     * require any arguments to be constructed, but it is not trivially
     * copyable. This is the case for example of dealii::Triangulation<2>.
     */
    template <typename T>
    static NodeObjectInitializer &
    register_type()
    {
      auto &initializer                       = register_json_header<T>();
      initializer.json_serializer["run_type"] = "empty_constructor";

      // Add to the initializer the emtpy executor.
      initializer.executor =
        [](std::vector<std::shared_ptr<NodeObject>> args) -> std::any {
        AssertThrow(args.size() == 0,
                    dealii::ExcMessage("Wrong number of arguments."));
        return std::any(std::make_shared<T>());
      };
      return initializer;
    }


    /**
     * Register an abstract type. This is a type that will never be constructed.
     */
    template <typename T>
    static NodeObjectInitializer &
    register_abstract_type()
    {
      auto &initializer                       = register_json_header<T>();
      initializer.json_serializer["run_type"] = "none";

      // Add to the initializer the emtpy executor.
      initializer.executor =
        [](std::vector<std::shared_ptr<NodeObject>>) -> std::any {
        return std::any(std::shared_ptr<T>());
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
    static NodeObjectInitializer &
    register_derived_type()
    {
      auto &initializer      = register_type<T>();
      auto &base_initializer = register_abstract_type<B>();

      base_initializer.json_serializer["derived"].push_back(
        initializer.json_serializer["type_hash"]);

      initializer.json_serializer["base"] =
        base_initializer.json_serializer["type_hash"];

      // Add the conversion to the base class.
      initializer.to_base = [](std::any a) -> std::any {
        std::cout << "Cast to derived class " << boost::core::type_name<T>()
                  << std::endl;
        auto ptr = std::any_cast<std::shared_ptr<T>>(a);
        std::cout << "Cast to base class " << boost::core::type_name<B>()
                  << std::endl;
        auto ptrB = std::static_pointer_cast<B>(ptr);
        std::cout << "Return std::any of base class" << std::endl;
        return std::any(ptrB);
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
    static NodeObjectInitializer &
    register_type(const std::vector<std::string> &arg_names)
    {
      auto &initializer = register_json_header<T, Args...>(arg_names);
      initializer.json_serializer["run_type"] = "constructor";

      // And the executor.
      initializer.executor =
        [](std::vector<std::shared_ptr<NodeObject>> args) -> std::any {
        AssertThrow(args.size() == sizeof...(Args),
                    dealii::ExcMessage("Wrong number of arguments."));
        auto tuple = cast_args<Args...>(args);
        return std::apply(
          [](auto &&...unpackedArgs) {
            return std::any(std::make_shared<T>(*unpackedArgs...));
          },
          tuple);
      };
      return initializer;
    }

    /**
     * Register a non-trivially constructible type T derived from type B.
     */
    template <typename B, typename T, typename... Args>
    static NodeObjectInitializer &
    register_derived_type(const std::vector<std::string> &arg_names)
    {
      auto &initializer      = register_type<T, Args...>(arg_names);
      auto &base_initializer = register_abstract_type<B>();

      base_initializer.json_serializer["derived"].push_back(
        initializer.json_serializer["type_hash"]);

      initializer.json_serializer["base"] =
        base_initializer.json_serializer["type_hash"];

      // Add the conversion to the base class.
      initializer.to_base = [](std::any a) -> std::any {
        return std::any(
          std::static_pointer_cast<B>(std::any_cast<std::shared_ptr<T>>(a)));
      };
      return initializer;
    }

    /**
     * Same as above, for objects that require a single argument.
     */
    template <typename T, typename Arg>
    static NodeObjectInitializer &
    register_type(const std::string &arg_name)
    {
      return register_type<T, Arg>(std::vector<std::string>{{arg_name}});
    }

    /**
     * Same as above, for objects that require a single argument.
     */
    template <typename B, typename T, typename Arg>
    static NodeObjectInitializer &
    register_derived_type(const std::string &arg_name)
    {
      return register_derived_type<B, T, Arg>(
        std::vector<std::string>{{arg_name}});
    }

    /**
     * A general pointer-to-member-function type.
     */
    template <typename T, typename ReturnType, typename... Args>
    using MethodType = dealii::Utilities::
      MutableBind<std::shared_ptr<T>, std::shared_ptr<ReturnType>, Args...>;

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
            std::remove_cv_t<std::remove_reference_t<Args>>...>(arg_names);
          initializer.json_serializer["run_type"]    = "void_method";
          initializer.json_serializer["method_name"] = method_name;


          // Add the method to the initializer
          initializer.executor =
            [ptr](std::vector<std::shared_ptr<NodeObject>> args) -> std::any {
            AssertThrow(args.size() == 1 + sizeof...(Args),
                        dealii::ExcMessage("Wrong number of arguments."));
            auto &obj = args[0]->get<T>();
            args.erase(args.begin()); // remove the first element

            auto tuple = cast_args<Args...>(args);

            std::apply(
              [&obj, ptr](auto &&...unpackedArgs) {
                (obj.*ptr)(*unpackedArgs...);
              },
              tuple);
            return std::any(ptr);
          };
        }
      else
        {
          auto &initializer = register_json_header<
            ThisMethod,
            T,
            ReturnType,
            std::remove_cv_t<std::remove_reference_t<Args>>...>(arg_names);
          initializer.json_serializer["run_type"]    = "method";
          initializer.json_serializer["method_name"] = method_name;

          // Add to the initializer the emtpy executor.
          initializer.executor =
            [ptr](std::vector<std::shared_ptr<NodeObject>> args) -> std::any {
            AssertThrow(args.size() == 2 + sizeof...(Args),
                        dealii::ExcMessage("Wrong number of arguments."));
            auto &obj = args[0]->get<T>();
            auto &ret = args[1]->get<ReturnType>();
            args.erase(args.begin()); // remove the class
            args.erase(args.begin()); // remove the return type

            auto tuple = cast_args<Args...>(args);
            ret        = std::apply(
              [&obj, ptr](auto &&...unpackedArgs) {
                (obj.*ptr)(*unpackedArgs...);
              },
              tuple);
            return std::any(ptr);
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
          auto &initializer = register_json_header<
            ThisMethod,
            T,
            std::remove_cv_t<std::remove_reference_t<Args>>...>(arg_names);
          initializer.json_serializer["run_type"]    = "void_const_method";
          initializer.json_serializer["method_name"] = method_name;


          // Add the method to the initializer
          initializer.executor =
            [ptr](std::vector<std::shared_ptr<NodeObject>> args) -> std::any {
            AssertThrow(args.size() == 1 + sizeof...(Args),
                        dealii::ExcMessage("Wrong number of arguments."));
            auto &obj = args[0]->get<T>();
            args.erase(args.begin()); // remove the first element

            auto tuple = cast_args<Args...>(args);

            std::apply(
              [&obj, ptr](auto &&...unpackedArgs) {
                (obj.*ptr)(*unpackedArgs...);
              },
              tuple);
            return std::any(ptr);
          };
        }
      else
        {
          auto &initializer = register_json_header<
            ThisMethod,
            T,
            ReturnType,
            std::remove_cv_t<std::remove_reference_t<Args>>...>(arg_names);
          initializer.json_serializer["run_type"]    = "const_method";
          initializer.json_serializer["method_name"] = method_name;

          // Add to the initializer the emtpy executor.
          initializer.executor =
            [ptr](std::vector<std::shared_ptr<NodeObject>> args) -> std::any {
            AssertThrow(args.size() == 2 + sizeof...(Args),
                        dealii::ExcMessage("Wrong number of arguments."));
            auto &obj = args[0]->get<T>();
            auto &ret = args[1]->get<ReturnType>();
            args.erase(args.begin()); // remove the class
            args.erase(args.begin()); // remove the return type

            auto tuple = cast_args<Args...>(args);
            ret        = std::apply(
              [&obj, ptr](auto &&...unpackedArgs) {
                (obj.*ptr)(*unpackedArgs...);
              },
              tuple);
            return std::any(ptr);
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
    register_function(ReturnType (*ptr)(Args...),
                      std::vector<std::string> arg_names)
    {
      using ThisMethod              = decltype(ptr);
      constexpr bool return_is_void = std::is_same_v<ReturnType, void>;
      AssertThrow(arg_names.size() > 0,
                  dealii::ExcMessage("You must provide at least the name of "
                                     "the function as the first argument."));
      auto method_name = arg_names[0];
      arg_names.erase(arg_names.begin());

      if constexpr (return_is_void)
        {
          AssertThrow(arg_names.size() == sizeof...(Args),
                      dealii::ExcMessage("Wrong number of arguments."));
          auto &initializer = register_json_header<
            ThisMethod,
            std::remove_cv_t<std::remove_reference_t<Args>>...>(arg_names);
          initializer.json_serializer["run_type"]    = "void_function";
          initializer.json_serializer["method_name"] = method_name;

          // Add the method to the initializer
          initializer.executor =
            [ptr](std::vector<std::shared_ptr<NodeObject>> args) -> std::any {
            AssertThrow(args.size() == sizeof...(Args),
                        dealii::ExcMessage("Wrong number of arguments."));

            auto tuple = cast_args<Args...>(args);

            std::apply([ptr](auto &&...unpackedArgs) { ptr(*unpackedArgs...); },
                       tuple);
            return std::any(ptr);
          };
        }
      else
        {
          auto &initializer = register_json_header<
            ThisMethod,
            ReturnType,
            std::remove_cv_t<std::remove_reference_t<Args>>...>(arg_names);
          initializer.json_serializer["run_type"]    = "function";
          initializer.json_serializer["method_name"] = method_name;

          // Add the method to the initializer
          initializer.executor =
            [ptr](std::vector<std::shared_ptr<NodeObject>> args) -> std::any {
            AssertThrow(args.size() == 1 + sizeof...(Args),
                        dealii::ExcMessage("Wrong number of arguments."));
            auto &ret = args[0]->get<ReturnType>();
            args.erase(args.begin()); // remove the first element

            auto tuple = cast_args<Args...>(args);

            ret = std::apply(
              [ptr](auto &&...unpackedArgs) { ptr(*unpackedArgs...); }, tuple);
            return std::any(ptr);
          };
        }
    }

    /**
     * Get a shared pointer of the stored object.
     *
     * Will throw if the object is not ready, or if the stored object is not
     * of the requested type.
     */
    template <typename T>
    std::shared_ptr<T>
    get_shared()
    {
      AssertThrow(ready(), dealii::ExcMessage("Object is not ready."));
      using type = std::remove_cv_t<std::remove_reference_t<T>>;
      std::shared_ptr<type> ptr;

      if (hash() != coral::hash<type>())
        {
          // If the object is not of the requested type, try to convert it
          // to the base class, using the right initializer.
          auto &j = initializer.json_serializer;
          AssertThrow(j.contains("base") &&
                        (coral::hash<type>() == j.at("base")),
                      dealii::ExcMessage("Cannot cast object of type " +
                                         type_name() + " to object of type " +
                                         boost::core::type_name<type>() + "."));
          auto new_object = initializer.to_base(object);
          AssertThrow(new_object.has_value(),
                      dealii::ExcMessage("New object does not have value."));
          AssertThrow(coral::hash(new_object) == j.at("base"),
                      dealii::ExcMessage(
                        "New object does not have the right hash."));
          ptr = std::any_cast<std::shared_ptr<type>>(new_object);
        }
      else
        try
          {
            ptr = std::any_cast<std::shared_ptr<type>>(object);
          }
        catch (...)
          {
            AssertThrow(false,
                        dealii::ExcMessage(
                          "Could not cast object to shared pointer of type " +
                          boost::core::type_name<type>() +
                          " from object of type " +
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
      using type = std::remove_cv_t<std::remove_reference_t<T>>;
      std::shared_ptr<const type> ptr;
      if (hash() != coral::hash<type>())
        {
          // If the object is not of the requested type, try to convert it
          // to the base class, using the right initializer.
          auto &j = initializer.json_serializer;
          AssertThrow(j.contains("base") &&
                        (coral::hash<type>() == j.at("base")),
                      dealii::ExcMessage("Cannot cast object of type " +
                                         type_name() + " to object of type " +
                                         boost::core::type_name<type>() + "."));
          auto new_object = initializer.to_base(object);
          AssertThrow(new_object.has_value(),
                      dealii::ExcMessage("New object does not have value."));
          AssertThrow(coral::hash(new_object) == j.at("base"),
                      dealii::ExcMessage(
                        "New object does not have the right hash."));
          ptr = std::any_cast<std::shared_ptr<const type>>(new_object);
        }
      else
        try
          {
            ptr = std::any_cast<std::shared_ptr<const type>>(object);
          }
        catch (...)
          {
            AssertThrow(false,
                        dealii::ExcMessage(
                          "Could not cast object to shared pointer of type " +
                          boost::core::type_name<type>() +
                          " from object of type " +
                          boost::core::demangle(object.type().name()) + "."));
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
      auto &j = initializer.json_serializer;
      if (initializer.to_string && object.has_value())
        j["value"] = initializer.to_string(object);
      return j;
    }

    /**
     * Get the hash of the stored object.
     */
    std::string
    hash() const
    {
      if (object.has_value())
        {
          // The object is initialized. Check if this is consistent with
          // the initializer, and return its hash.
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
     * Anything required to build a std::shared_ptr<T> object, and to
     * manipulate it.
     */
    NodeObjectInitializer initializer;

    /**
     * The arguments of this node to other nodes.
     */
    std::vector<std::shared_ptr<NodeObject>> arguments;

    /**
     * A list of all known types and their initializers.
     */
    static std::map<std::string, NodeObjectInitializer> initializers;
  };

  /**
   * json serialization of a NodeObject.
   */
  inline void
  to_json(json &j, const NodeObjectPtr &obj)
  {
    j = obj->get_info();
    if (obj->ready() && j.contains("value"))
      {
        j["value"] = obj->to_string();
      }
  }

  /**
   * json deserialization of a NodeObject.
   */
  inline void
  from_json(const json &j, NodeObjectPtr &obj)
  {
    AssertThrow(j.contains("type_hash"),
                dealii::ExcMessage(
                  "The json does not contain a hash_type entry. Bailing out."));
    obj = make_node(j.at("type_hash").get<std::string>());
    if (j["run_type"] == "elementary_constructor" ||
        j["run_type"] == "empty_constructor")
      (*obj)();
    if (j.contains("value"))
      {
        obj->parse_string(j.at("value").get<std::string>());
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
        // std::any_cast<std::shared_ptr<
        //   std::remove_const_t<std::remove_reference_t<Args>>>>(*args[Is])...);
      }
    catch (const std::bad_any_cast &e)
      {
        AssertThrow(false, dealii::ExcMessage(e.what()));
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
} // namespace coral
#endif