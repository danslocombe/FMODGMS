#pragma once

inline bool stringEqualIgnoreCase(const std::string_view& x, const std::string_view& y)
{
    return (x.size() == y.size()) && (_strnicmp(x.data(), y.data(), x.size()) == 0);
}
