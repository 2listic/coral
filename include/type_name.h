#ifndef BOOST_CORE_TYPE_NAME_HPP_INCLUDED
#define BOOST_CORE_TYPE_NAME_HPP_INCLUDED

// MS compatible compilers support #pragma once

#if defined(_MSC_VER) && (_MSC_VER >= 1020)
#  pragma once
#endif

// std::string boost::core::type_name<T>()
//
// Copyright 2021 Peter Dimov
// Distributed under the Boost Software License, Version 1.0.
// https://www.boost.org/LICENSE_1_0.txt

#include <boost/config.hpp>
#include <boost/core/demangle.hpp>
#include <boost/type_traits/is_same.hpp>

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iosfwd>
#include <memory>
#include <string>
#include <utility>
#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)
#  include <string_view>
#endif

namespace boost
{
  namespace core
  {
    namespace detail
    {

      // tn_identity

      template <class T>
      struct tn_identity
      {
        typedef T type;
      };

      // tn_remove_prefix

      inline bool
      tn_remove_prefix(std::string &str, const char *prefix)
      {
        std::size_t n = std::strlen(prefix);

        if (str.substr(0, n) == prefix)
          {
            str = str.substr(n);
            return true;
          }
        else
          {
            return false;
          }
      }

#if !defined(BOOST_NO_TYPEID)

      // typeid_name

      inline std::string
      fix_typeid_name(const char *n)
      {
        std::string r = boost::core::demangle(n);

#  if defined(_MSC_VER)

        tn_remove_prefix(r, "class ");
        tn_remove_prefix(r, "struct ");
        tn_remove_prefix(r, "enum ");

#  endif

        // libc++ inline namespace

        if (tn_remove_prefix(r, "std::__1::"))
          {
            r = "std::" + r;
          }

        // libstdc++ inline namespace

        if (tn_remove_prefix(r, "std::__cxx11::"))
          {
            r = "std::" + r;
          }

#  if defined(BOOST_MSVC) && BOOST_MSVC == 1600

        // msvc-10.0 puts TR1 things in std::tr1

        if (tn_remove_prefix(r, "std::tr1::"))
          {
            r = "std::" + r;
          }

#  endif

        return r;
      }

      template <class T>
      std::string
      typeid_name()
      {
        return fix_typeid_name(typeid(T).name());
      }

      // template names

      template <class T>
      std::string
      class_template_name()
      {
#  if defined(BOOST_GCC)

        std::string r = typeid_name<T()>();

#  else

        std::string r = typeid_name<T *>();

#  endif
        return r.substr(0, r.find('<'));
      }

      template <class T>
      std::string
      sequence_template_name()
      {
        return detail::class_template_name<T>();
      }

      template <class T>
      std::string
      set_template_name()
      {
        return detail::class_template_name<T>();
      }

      template <class T>
      std::string
      map_template_name()
      {
        return detail::class_template_name<T>();
      }

      template <class T>
      std::string
      array_template_name()
      {
        return detail::class_template_name<T>();
      }

#else // #if !defined(BOOST_NO_TYPEID)

      template <class T>
      std::string
      typeid_name()
      {
        return "_Tp";
      }

      template <class T>
      std::string
      class_template_name()
      {
        return "_Tm";
      }

      template <class T>
      std::string
      sequence_template_name()
      {
        return "_Sq";
      }

      template <class T>
      std::string
      set_template_name()
      {
        return "_St";
      }

      template <class T>
      std::string
      map_template_name()
      {
        return "_Mp";
      }

      template <class T>
      std::string
      array_template_name()
      {
        return "_Ar";
      }

#endif

      // tn_to_string

#if defined(BOOST_MSVC)
#  pragma warning(push)
#  pragma warning(disable : 4996)
#endif

      inline std::string
      tn_to_string(std::size_t n)
      {
        char       buffer[32];
        const auto written = std::snprintf(buffer,
                                           sizeof(buffer),
                                           "%lu",
                                           static_cast<unsigned long>(n));

        if (written < 0)
          return {};

        return std::string(buffer,
                           static_cast<std::size_t>(std::min<int>(
                             written, static_cast<int>(sizeof(buffer) - 1))));
      }

#if defined(BOOST_MSVC)
#  pragma warning(pop)
#endif

      // tn_holder

      template <class T>
      struct tn_holder
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return typeid_name<T>() + suffix;
        }
      };

      // integrals

      template <>
      struct tn_holder<bool>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "bool" + suffix;
        }
      };

      template <>
      struct tn_holder<char>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "char" + suffix;
        }
      };

      template <>
      struct tn_holder<signed char>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "signed char" + suffix;
        }
      };

      template <>
      struct tn_holder<unsigned char>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "unsigned char" + suffix;
        }
      };

      template <>
      struct tn_holder<short>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "short" + suffix;
        }
      };

      template <>
      struct tn_holder<unsigned short>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "unsigned short" + suffix;
        }
      };

      template <>
      struct tn_holder<int>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "int" + suffix;
        }
      };

      template <>
      struct tn_holder<unsigned>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "unsigned" + suffix;
        }
      };

      template <>
      struct tn_holder<long>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "long" + suffix;
        }
      };

      template <>
      struct tn_holder<unsigned long>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "unsigned long" + suffix;
        }
      };

      template <>
      struct tn_holder<boost::long_long_type>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "long long" + suffix;
        }
      };

      template <>
      struct tn_holder<boost::ulong_long_type>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "unsigned long long" + suffix;
        }
      };

      template <>
      struct tn_holder<wchar_t>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "wchar_t" + suffix;
        }
      };

#if !defined(BOOST_NO_CXX11_CHAR16_T)

      template <>
      struct tn_holder<char16_t>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "char16_t" + suffix;
        }
      };

#endif

#if !defined(BOOST_NO_CXX11_CHAR32_T)

      template <>
      struct tn_holder<char32_t>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "char32_t" + suffix;
        }
      };

#endif

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L

      template <>
      struct tn_holder<char8_t>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "char8_t" + suffix;
        }
      };

#endif

#if defined(__cpp_lib_byte) && __cpp_lib_byte >= 201603L

      template <>
      struct tn_holder<std::byte>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "std::byte" + suffix;
        }
      };

#endif

      // floating point

      template <>
      struct tn_holder<float>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "float" + suffix;
        }
      };

      template <>
      struct tn_holder<double>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "double" + suffix;
        }
      };

      template <>
      struct tn_holder<long double>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "long double" + suffix;
        }
      };

      // void

      template <>
      struct tn_holder<void>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "void" + suffix;
        }
      };

      // nullptr_t

#if !defined(BOOST_NO_CXX11_NULLPTR)

      template <>
      struct tn_holder<std::nullptr_t>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return "std::nullptr_t" + suffix;
        }
      };

#endif

      // cv

      template <class T>
      struct tn_holder<const T>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return tn_holder<T>::type_name(" const" + suffix);
        }
      };

      template <class T>
      struct tn_holder<volatile T>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return tn_holder<T>::type_name(" volatile" + suffix);
        }
      };

      template <class T>
      struct tn_holder<T const volatile>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return tn_holder<T>::type_name(" const volatile" + suffix);
        }
      };

      // refs

      template <class T>
      struct tn_holder<T &>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return tn_holder<T>::type_name("&" + suffix);
        }
      };

#if !defined(BOOST_NO_CXX11_RVALUE_REFERENCES)

      template <class T>
      struct tn_holder<T &&>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return tn_holder<T>::type_name("&&" + suffix);
        }
      };

#endif

      // function types

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

      // tn_add_each

      template <class T>
      int
      tn_add_each_impl(std::string &st)
      {
        if (!st.empty())
          st += ", ";
        st += tn_holder<T>::type_name("");
        return 0;
      }

      template <class... T>
      std::string
      tn_add_each()
      {
        std::string st;

        typedef int A[sizeof...(T) + 1];
        (void)A{0, tn_add_each_impl<T>(st)...};

        return st;
      }

      template <class R, class... A>
      std::string
      function_type_name(tn_identity<R(A...)>,
                         const std::string &trailer,
                         const std::string &suffix)
      {
        std::string r = tn_holder<R>::type_name("");

        if (!suffix.empty())
          {
            r += '(';

            if (suffix[0] == ' ')
              {
                r += suffix.substr(1);
              }
            else
              {
                r += suffix;
              }

            r += ')';
          }

        r += '(' + tn_add_each<A...>() + ')';
        r += trailer;

        return r;
      }

      template <class R, class... A>
      struct tn_holder<R(A...)>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(), "", suffix);
        }
      };

#  if !defined(BOOST_MSVC) || BOOST_MSVC >= 1900

      template <class R, class... A>
      struct tn_holder<R(A...) const>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " const",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) volatile>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " volatile",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) const volatile>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " const volatile",
                                            suffix);
        }
      };

#  endif

#  if !defined(BOOST_NO_CXX11_REF_QUALIFIERS)

      template <class R, class... A>
      struct tn_holder<R(A...) &>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " &",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) const &>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " const &",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) volatile &>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " volatile &",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) const volatile &>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " const volatile &",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) &&>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " &&",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) const &&>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " const &&",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) volatile &&>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " volatile &&",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) const volatile &&>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " const volatile &&",
                                            suffix);
        }
      };

#  endif

#  if defined(__cpp_noexcept_function_type) || \
    defined(_NOEXCEPT_TYPES_SUPPORTED)

      template <class R, class... A>
      struct tn_holder<R(A...) noexcept>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " noexcept",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) const noexcept>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " const noexcept",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) volatile noexcept>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " volatile noexcept",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) const volatile noexcept>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " const volatile noexcept",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) & noexcept>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " & noexcept",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) const & noexcept>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " const & noexcept",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) volatile & noexcept>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " volatile & noexcept",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) const volatile & noexcept>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " const volatile & noexcept",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) && noexcept>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " && noexcept",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) const && noexcept>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " const && noexcept",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) volatile && noexcept>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " volatile && noexcept",
                                            suffix);
        }
      };

      template <class R, class... A>
      struct tn_holder<R(A...) const volatile && noexcept>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::function_type_name(tn_identity<R(A...)>(),
                                            " const volatile && noexcept",
                                            suffix);
        }
      };

#  endif

#endif // #if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

      // pointers

      template <class T>
      struct tn_holder<T *>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return tn_holder<T>::type_name("*" + suffix);
        }
      };

      // arrays

      template <class T>
      std::pair<std::string, std::string>
      array_prefix_suffix(tn_identity<T>)
      {
        return std::pair<std::string, std::string>(tn_holder<T>::type_name(""),
                                                   "");
      }

      template <class T, std::size_t N>
      std::pair<std::string, std::string>
      array_prefix_suffix(tn_identity<T[N]>)
      {
        std::pair<std::string, std::string> r =
          detail::array_prefix_suffix(tn_identity<T>());

        r.second = '[' + tn_to_string(N) + ']' + r.second;

        return r;
      }

      template <class T>
      std::string
      array_type_name(tn_identity<T[]>, const std::string &suffix)
      {
        std::pair<std::string, std::string> r =
          detail::array_prefix_suffix(tn_identity<T>());

        if (suffix.empty())
          {
            return r.first + "[]" + r.second;
          }
        else
          {
            return r.first + '(' + suffix + ")[]" + r.second;
          }
      }

      template <class T>
      struct tn_holder<T[]>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::array_type_name(tn_identity<T[]>(), suffix);
        }
      };

      template <class T>
      struct tn_holder<const T[]>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::array_type_name(tn_identity<const T[]>(), suffix);
        } // namespace detail
      };  // namespace core

      template <class T>
      struct tn_holder<volatile T[]>
      {
        static std::string
        type_name(const std::string &suffix)
        {
          return detail::array_type_name(tn_identity<volatile T[]>(), suffix);
        } // namespace boost
      };

      template <class T>
      struct tn_holder<T const volatile[]>{
        static std::string type_name(const std::string &suffix){
          return detail::array_type_name(tn_identity<T const volatile[]>(),
                                         suffix);
    } // namespace detail
  };  // namespace core

  template <class T, std::size_t N>
  std::string
  array_type_name(tn_identity<T[N]>, const std::string &suffix)
  {
    std::pair<std::string, std::string> r =
      detail::array_prefix_suffix(tn_identity<T[N]>());

    if (suffix.empty())
      {
        return r.first + r.second;
      }
    else
      {
        return r.first + '(' + suffix + ")" + r.second;
      }
  }

  template <class T, std::size_t N>
  struct tn_holder<T[N]>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return detail::array_type_name(tn_identity<T[N]>(), suffix);
    }
  };

  template <class T, std::size_t N>
  struct tn_holder<const T[N]>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return detail::array_type_name(tn_identity<const T[N]>(), suffix);
    }
  };

  template <class T, std::size_t N>
  struct tn_holder<volatile T[N]>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return detail::array_type_name(tn_identity<volatile T[N]>(), suffix);
    }
  };

  template <class T, std::size_t N>
  struct tn_holder<T const volatile [N]> {
    static std::string type_name(std::string const &suffix)
    {
      return detail::array_type_name(tn_identity<T const volatile[N]>(),
                                     suffix);
    }
  };

  // pointers to members

  template <class R, class T>
  struct tn_holder<R T::*>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return tn_holder<R>::type_name(' ' + tn_holder<T>::type_name("") + "::*" +
                                     suffix);
    }
  };

#if defined(BOOST_MSVC) && BOOST_MSVC < 1900 && \
  !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

  template <class R, class T, class... A>
  struct tn_holder<R (T::*)(A...)>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return detail::function_type_name(tn_identity<R(A...)>(),
                                        "",
                                        ' ' + tn_holder<T>::type_name("") +
                                          "::*" + suffix);
    }
  };

  template <class R, class T, class... A>
  struct tn_holder<R (T::*)(A...) const>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return detail::function_type_name(tn_identity<R(A...)>(),
                                        " const",
                                        ' ' + tn_holder<T>::type_name("") +
                                          "::*" + suffix);
    }
  };

  template <class R, class T, class... A>
  struct tn_holder<R (T::*)(A...) volatile>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return detail::function_type_name(tn_identity<R(A...)>(),
                                        " volatile",
                                        ' ' + tn_holder<T>::type_name("") +
                                          "::*" + suffix);
    }
  };

  template <class R, class T, class... A>
  struct tn_holder<R (T::*)(A...) const volatile>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return detail::function_type_name(tn_identity<R(A...)>(),
                                        " const volatile",
                                        ' ' + tn_holder<T>::type_name("") +
                                          "::*" + suffix);
    }
  };

#endif

  // strings

  template <template <class Ch, class Tr, class A> class L, class Ch>
  struct tn_holder<L<Ch, std::char_traits<Ch>, std::allocator<Ch>>>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      std::string tn = sequence_template_name<
        L<Ch, std::char_traits<Ch>, std::allocator<Ch>>>();
      return tn + '<' + tn_holder<Ch>::type_name("") + '>' + suffix;
    }
  };

  template <>
  struct tn_holder<std::string>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return "std::string" + suffix;
    }
  };

  template <>
  struct tn_holder<std::wstring>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return "std::wstring" + suffix;
    }
  };

#if !defined(BOOST_NO_CXX11_CHAR16_T)

  template <>
  struct tn_holder<std::u16string>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return "std::u16string" + suffix;
    }
  };

#endif

#if !defined(BOOST_NO_CXX11_CHAR32_T)

  template <>
  struct tn_holder<std::u32string>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return "std::u32string" + suffix;
    }
  };

#endif

#if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L

  template <>
  struct tn_holder<std::basic_string<char8_t>>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return "std::u8string" + suffix;
    }
  };

#endif

  // string views (et al)

  template <template <class Ch, class Tr> class L, class Ch>
  struct tn_holder<L<Ch, std::char_traits<Ch>>>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      std::string tn = sequence_template_name<L<Ch, std::char_traits<Ch>>>();
      return tn + '<' + tn_holder<Ch>::type_name("") + '>' + suffix;
    }
  };

  // needed for libstdc++
  template <>
  struct tn_holder<std::ostream>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return "std::ostream" + suffix;
    }
  };

#if !defined(BOOST_NO_CXX17_HDR_STRING_VIEW)

  template <>
  struct tn_holder<std::string_view>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return "std::string_view" + suffix;
    }
  };

  template <>
  struct tn_holder<std::wstring_view>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return "std::wstring_view" + suffix;
    }
  };

#  if !defined(BOOST_NO_CXX11_CHAR16_T)

  template <>
  struct tn_holder<std::u16string_view>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return "std::u16string_view" + suffix;
    }
  };

#  endif

#  if !defined(BOOST_NO_CXX11_CHAR32_T)

  template <>
  struct tn_holder<std::u32string_view>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return "std::u32string_view" + suffix;
    }
  };

#  endif

#  if defined(__cpp_char8_t) && __cpp_char8_t >= 201811L

  template <>
  struct tn_holder<std::basic_string_view<char8_t>>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      return "std::u8string_view" + suffix;
    }
  };

#  endif

#endif

  // class templates

#if !defined(BOOST_NO_CXX11_VARIADIC_TEMPLATES)

  template <template <class...> class L, class... T>
  struct tn_holder<L<T...>>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      std::string tn = detail::class_template_name<L<T...>>();
      std::string st = tn_add_each<T...>();

      return tn + '<' + st + '>' + suffix;
    }
  };

#else

  template <template <class T1> class L, class T1>
  struct tn_holder<L<T1>>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      std::string tn = detail::class_template_name<L<T1>>();
      return tn + '<' + tn_holder<T1>::type_name("") + '>' + suffix;
    }
  };

  template <template <class T1, class T2> class L, class T1, class T2>
  struct tn_holder<L<T1, T2>>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      std::string tn = detail::class_template_name<L<T1, T2>>();
      return tn + '<' + tn_holder<T1>::type_name("") + ", " +
             tn_holder<T2>::type_name("") + '>' + suffix;
    }
  };

#endif

  // sequence containers

  template <template <class T, class A> class L, class T>
  struct tn_holder<L<T, std::allocator<T>>>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      std::string tn =
        detail::sequence_template_name<L<T, std::allocator<T>>>();
      return tn + '<' + tn_holder<T>::type_name("") + '>' + suffix;
    }
  };

  // set

  template <template <class T, class Pr, class A> class L, class T>
  struct tn_holder<L<T, std::less<T>, std::allocator<T>>>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      std::string tn =
        detail::set_template_name<L<T, std::less<T>, std::allocator<T>>>();
      return tn + '<' + tn_holder<T>::type_name("") + '>' + suffix;
    }
  };

  // map

  template <template <class T, class U, class Pr, class A> class L,
            class T,
            class U>
  struct tn_holder<L<T, U, std::less<T>, std::allocator<std::pair<const T, U>>>>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      std::string tn = detail::map_template_name<
        L<T, U, std::less<T>, std::allocator<std::pair<const T, U>>>>();
      return tn + '<' + tn_holder<T>::type_name("") + ", " +
             tn_holder<U>::type_name("") + '>' + suffix;
    }
  };

#if !defined(BOOST_NO_CXX11_HDR_FUNCTIONAL)

  // unordered_set

  template <template <class T, class H, class Eq, class A> class L, class T>
  struct tn_holder<L<T, std::hash<T>, std::equal_to<T>, std::allocator<T>>>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      std::string tn = detail::set_template_name<
        L<T, std::hash<T>, std::equal_to<T>, std::allocator<T>>>();
      return tn + '<' + tn_holder<T>::type_name("") + '>' + suffix;
    }
  };

  // unordered_map

  template <template <class T, class U, class H, class Eq, class A> class L,
            class T,
            class U>
  struct tn_holder<L<T,
                     U,
                     std::hash<T>,
                     std::equal_to<T>,
                     std::allocator<std::pair<const T, U>>>>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      std::string tn =
        detail::map_template_name<L<T,
                                    U,
                                    std::hash<T>,
                                    std::equal_to<T>,
                                    std::allocator<std::pair<const T, U>>>>();
      return tn + '<' + tn_holder<T>::type_name("") + ", " +
             tn_holder<U>::type_name("") + '>' + suffix;
    }
  };

#endif

  // array

  template <template <class T, std::size_t N> class L, class T, std::size_t N>
  struct tn_holder<L<T, N>>
  {
    static std::string
    type_name(const std::string &suffix)
    {
      std::string tn = detail::array_template_name<L<T, N>>();
      return tn + '<' + tn_holder<T>::type_name("") + ", " + tn_to_string(N) +
             '>' + suffix;
    }
  };

} // namespace boost

template <class T>
std::string
type_name()
{
  return core::detail::tn_holder<T>::type_name("");
}

} // namespace core
} // namespace boost

#endif // #ifndef BOOST_CORE_TYPE_NAME_HPP_INCLUDED
