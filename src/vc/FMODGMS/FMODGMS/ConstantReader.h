#pragma once

#include <string>
#include <string_view>
#include <variant>
#include <unordered_map>
#include <shared_mutex>
#include <windows.h>
#include <optional>

class ConstantObj;

typedef std::variant<bool, double, std::string, std::shared_ptr<const ConstantObj>> _ConstantReader_Constant;

class ConstantObj
{
public:
    ConstantObj(std::unordered_map<std::string_view, _ConstantReader_Constant> fields);
    ConstantObj() = default;

    bool GetBool(const std::string_view& name) const;
    int GetInt(const std::string_view& name) const;
    uint32_t GetUint(const std::string_view& name) const;
    double GetDouble(const std::string_view& name) const;
    std::string GetString(const std::string_view& name) const;
    std::shared_ptr<const ConstantObj> GetObj(const std::string_view& name) const;

private:
    std::optional<_ConstantReader_Constant> FindConstant(const std::string_view& name) const;
    std::unordered_map<std::string_view, _ConstantReader_Constant> m_values;
};


class ConstantReader
{
public:
    ConstantReader(const std::string_view& path);
    ~ConstantReader();

    void Refresh(bool force);

    bool GetBool(const std::string_view& name) const;
    int GetInt(const std::string_view& name) const;
    uint32_t GetUint(const std::string_view& name) const;
    double GetDouble(const std::string_view& name) const;
    std::string GetString(const std::string_view& name) const;
    std::shared_ptr<const ConstantObj> GetObj(const std::string_view& name) const;


private:
    HANDLE m_handle;
    FILETIME m_lastModified;

    mutable std::shared_mutex m_rwMutex;

    std::string m_fileContents;

    static std::unordered_map<std::string_view, _ConstantReader_Constant> ParseValues(const std::string_view& str);
    static _ConstantReader_Constant ParseConstant(const std::string_view& str);
    static std::optional<std::pair<std::string_view, _ConstantReader_Constant>> ParseObject(uint32_t& i, const std::vector<std::string_view>& lines);

    ConstantObj m_baseObj;
    bool ShouldRebuild(LPFILETIME newLastWriteTime) const;
};

namespace Constants
{
    extern ConstantReader Globals;
}
