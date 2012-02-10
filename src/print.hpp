#pragma once

#include <type_traits>
#include <string>
#include <stdexcept>

// adapted from example given by Andre Alexandrescu
// at GoingNative 2012

#if DEBUG
#define ENABLE_CHECK_PR
#else
#undef ENABLE_CHECK_PR
#endif

namespace print {
    struct bad_format : public std::runtime_error {
        bad_format(const char* msg) : runtime_error(msg) {}
    };

    namespace internal {

        template <typename T>
        typename std::enable_if<std::is_integral<T>::value, long>::type
        normalizeArg(T arg) {
            return arg;
        }

        template <typename T>
        typename std::enable_if<std::is_floating_point<T>::value, double>::type
        normalizeArg(T arg) {
            return arg;
        }

        template <typename T>
        typename std::enable_if<std::is_pointer<T>::value, T>::type
        normalizeArg(T arg) {
            return arg;
        }

        inline
        const char* normalizeArg(const std::string& arg) {
            return arg.c_str();
        }

        inline
        const char* normalizeArg(bool arg) {
            return arg ? "true" : "false";
        }

        inline
        void check(const char* f) {
            for (; *f; ++f) {
                if (*f != '%' || *++f == '%')
                    continue;
                throw bad_format("missing arguments to pr()");
            }
        }

        inline
        void enforce(bool cond, const char* msg) {
            if (!cond)
                throw bad_format(msg);
        }

        inline
        const char* skip_modifiers(const char* f) {
            while (*f && (*f == '-' ||
                          *f == '+' ||
                          *f == ' ' ||
                          *f == '#' ||
                          *f == '*' ||
                          *f == '.' ||
                          *f == 'h' ||
                          *f == 'l' ||
                          *f == 'L' ||
                          *f == 't' ||
                          *f == 'z' ||
                          isdigit(*f)))
                ++f;
            return f;
        }

        template <class T, typename... Ts>
        void check(const char* f, const T& t,
                      const Ts&... ts) {
            for (; *f; ++f) {
                if (*f != '%' || *++f == '%') continue;

                // TODO: verify modifier stuff
                f = skip_modifiers(f);

                switch (*f) {
                default: throw bad_format("unknown format code");
                case 'f': case 'g': case 'e': case 'E': case 'G': case 'a': case 'A':
                    enforce(std::is_floating_point<T>::value, "non-floating point value to %(f) or %(g)");
                    break;
                case 's':
                    enforce(std::is_same<T, bool>::value ||
                            std::is_same<T, std::string>::value ||
                            std::is_same<T, const char*>::value, "non-string value to %(s)");
                    break;
                case 'd': case 'i':
                    enforce(std::is_signed<T>::value, "non-signed value to %(d) or %(i)");
                    break;
                case 'u':
                    enforce(std::is_unsigned<T>::value, "non-unsigned value to %(u)");
                    break;
                case 'p':
                    enforce(std::is_pointer<T>::value, "non-pointer value to %(p)");
                    break;
                case 'x': case 'X': case 'o':
                    enforce(std::is_integral<T>::value, "non-integral value to %(x) or %(X)");
                    break;
                case 'c':
                    enforce(std::is_integral<T>::value, "non-char value to %(c)");
                    break;
                case 'n':
                    enforce(false, "%(n) is not allowed");
                    break;
                }
                return check(++f, ts...); // variadic recursion
            }
            throw bad_format("missing arguments to pr()");
        }
    }

    template <typename... Ts>
    int fpr(FILE* fil, const char* f, const Ts&... ts) {
#ifdef ENABLE_CHECK_PR
        internal::check(f, ts...);
#endif
        return fprintf(fil, f, internal::normalizeArg(ts)...);
    }

    template <typename... Ts>
    int fpn(FILE* fil, const char* f, const Ts&... ts) {
        const int r = fpr(fil, f, ts...);
        fputs("\n", fil);
        return r+1;
    }

    template<typename... Ts>
    int pr(const char* f, const Ts&... ts) {
        return fpr(stdout, f, ts...);
    }

    template<typename... Ts>
    int pn(const char* f, const Ts&... ts) {
        return fpn(stdout, f, ts...);
    }

    /* not sure how to do sprintf typechecked in a good way
    template<typename... Ts>
    int spr(const char* f, const Ts&... ts) {
        #ifdef ENABLE_CHECK_PR
        internal::check(f, ts...);
        #endif
    }
    */

}
