#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <unordered_map>
#include <shared_mutex>
#include <windows.h>
#include <optional>

class ConstantReader
{
public:
    ConstantReader(const std::string_view& path);
    ~ConstantReader();

    void Refresh();

    bool GetBool(const std::string_view& name) const;
    int GetInt(const std::string_view& name) const;
    uint32_t GetUint(const std::string_view& name) const;
    double GetDouble(const std::string_view& name) const;
    std::string GetString(const std::string_view& name) const;

private:
    HANDLE m_handle;
    FILETIME m_lastModified;

    mutable std::shared_mutex m_rwMutex;

    std::string m_fileContents;

    typedef std::variant<bool, double, std::string> Constant;
    static Constant ParseConstant(const std::string_view& str);

    std::unordered_map<std::string_view, Constant> m_values;

    static std::unordered_map<std::string_view, Constant> ParseValues(const std::string_view& str);

    std::optional<Constant> FindConstant(const std::string_view& name) const;
    bool ShouldRebuild(LPFILETIME newLastWriteTime) const;
};

namespace Constants
{
    extern ConstantReader Globals;
}
