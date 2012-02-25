#pragma once

#include <string>
#include <sstream>

namespace util
{

    template <typename It>
    std::string join(const char* joint, It begin, const It& end) {
        std::stringstream ss;
        ss << *begin;
        ++begin;
        for (; begin != end; ++begin)
            ss << joint << *begin;
        //return std::move(ss.str());
        return ss.str();
    }

    template <typename Range>
    std::string join(const char* joint, const Range& range) {
        return join(joint, range.begin(), range.end());
    }

    template <typename Range>
    std::string join(const Range& range) {
        return join(", ", range.begin(), range.end());
    }

    template <typename It, typename Filter>
    std::string mapjoin(const char* joint, It begin, const It& end, const Filter& filter) {
        std::stringstream ss;
        ss << filter(*begin);
        ++begin;
        for (; begin != end; ++begin)
            ss << joint << filter(*begin);
        return ss.str();
    }

    template <typename It, typename Pred>
    std::string join_if(const char* joint, It begin, const It& end, const Pred& pred) {
        bool f = false;
        std::stringstream ss;
        for (; begin != end; ++begin) {
            if (pred(*begin)) {
                if (f)
                    ss << joint;
                ss << *begin;
                f = true;
            }
        }
        return ss.str();
    }

}
