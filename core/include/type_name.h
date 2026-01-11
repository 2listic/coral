#ifndef CORAL_TYPE_NAME_H
#define CORAL_TYPE_NAME_H

#include <string>
#include <typeinfo>

#if defined(__has_include)
#  if __has_include(<boost/core/demangle.hpp>)
#    include <boost/core/demangle.hpp>
#  endif
#endif

#if defined(__clang__) || defined(__GNUG__)
#  include <cxxabi.h>
#  include <cstdlib>
#endif

namespace boost
{
  namespace core
  {
    namespace coral_detail
    {
      inline void
      replace_all(std::string &s, const char *from, const char *to)
      {
        if (!from || !to)
          return;
        const std::string needle(from);
        if (needle.empty())
          return;
        const std::string repl(to);

        std::size_t pos = 0;
        while ((pos = s.find(needle, pos)) != std::string::npos)
          {
            s.replace(pos, needle.size(), repl);
            pos += repl.size();
          }
      }

      inline void
      normalize_type_string(std::string &s)
      {
        // Normalize common standard library inline namespaces.
        replace_all(s, "std::__1::", "std::");     // libc++
        replace_all(s, "std::__cxx11::", "std::"); // libstdc++

        // Make function types match a more common spelling.
        replace_all(s, " (", "(");

        // Common aliases used in tests and logs.
        if (s == "unsigned int")
          s = "unsigned";

        // std::string
        const bool is_basic_string_char =
          s.find("std::basic_string<char") == 0 &&
          s.find("std::char_traits<char>") != std::string::npos &&
          s.find("std::allocator<char>") != std::string::npos;
        if (is_basic_string_char)
          s = "std::string";

        // std::basic_ofstream<char>
        const bool is_basic_ofstream_char =
          s.find("std::basic_ofstream<char") == 0 &&
          s.find("std::char_traits<char>") != std::string::npos;
        if (is_basic_ofstream_char)
          s = "std::basic_ofstream<char>";

        // std::ostream
        replace_all(s,
                    "std::basic_ostream<char, std::char_traits<char>>",
                    "std::ostream");
      }

      inline std::string
      demangle_type_name(const char *name)
      {
        if (!name)
          return {};

#if defined(__clang__) || defined(__GNUG__)
        int   status    = 0;
        char *demangled = abi::__cxa_demangle(name, nullptr, nullptr, &status);
        std::string result =
          (status == 0 && demangled) ? std::string(demangled) :
                                       std::string(name);
        std::free(demangled);
        normalize_type_string(result);
        return result;
#else
        std::string result(name);
        normalize_type_string(result);
        return result;
#endif
      }
    } // namespace coral_detail

#if !defined(BOOST_CORE_DEMANGLE_HPP_INCLUDED)
    inline std::string
    demangle(const char *name)
    {
      return coral_detail::demangle_type_name(name);
    }
#  define BOOST_CORE_DEMANGLE_HPP_INCLUDED 1
#endif

#if !defined(BOOST_CORE_TYPE_NAME_HPP_INCLUDED)
    template <class T>
    inline std::string
    type_name()
    {
      return coral_detail::demangle_type_name(typeid(T).name());
    }
#  define BOOST_CORE_TYPE_NAME_HPP_INCLUDED 1
#endif

  } // namespace core
} // namespace boost

#endif // CORAL_TYPE_NAME_H
