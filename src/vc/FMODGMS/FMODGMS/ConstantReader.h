#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <unordered_map>
#include <shared_mutex>
#include <windows.h>

class ConstantReader
{
public:
    ConstantReader(const std::string_view& path);
    ~ConstantReader();

    void Refresh();

    int GetInt(const std::string_view& name) const;
    uint32_t GetUint(const std::string_view& name) const;
    double GetDouble(const std::string_view& name) const;
    const std::string_view& GetString(const std::string_view& name) const;

private:
    HANDLE m_handle;
    FILETIME m_lastModified;

    mutable std::shared_mutex m_rwLock;

    std::string m_fileContents;

    typedef std::variant<double, std::string_view> Constant;
    static Constant ParseConstant(const std::string_view& str);

    std::unordered_map<std::string_view, Constant> m_values;

    static std::unordered_map<std::string_view, Constant> ParseValues(const std::string_view& str);

    /*
    template<typename T>
    T Get(const std::string_view& name)
    {
        const auto found = m_values.find(name);
    }
    */

    bool ShouldRebuild();
    
    void ParseFile();
    time_t GetLastModifiedTime();
};

namespace Constants
{
    extern ConstantReader Globals;
}
