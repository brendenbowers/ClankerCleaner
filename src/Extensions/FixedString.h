#pragma once

#include <iostream>
#include <algorithm>
#include <string_view>


namespace ClankerCleaner::Extensions
{
    template<std::size_t N>
    struct FixedString 
    {
        char data[N]{};

        constexpr FixedString(const char (&str)[N])
        {
            std::copy_n(str, N, data);
        }

        constexpr operator std::string_view() const {
            return { data, N -1 };
        }
    };    
}


