#include "ConstantReader.h"
#include <handleapi.h> 
#include <fileapi.h> 
#include <optional>
#include <charconv>
#include "StringHelpers.h"

class shared_lock_guard
{
private:
    std::shared_mutex& m;

public:
    explicit shared_lock_guard(std::shared_mutex& m_):
        m(m_)
    {
        m.lock_shared();
    }
    ~shared_lock_guard()
    {
        m.unlock_shared();
    }
};

std::string ReadAll(HANDLE handle)
{
    SetFilePointer(handle, 0, NULL, FILE_BEGIN);

    std::string ret;

    constexpr size_t BUFFERSIZE = 1024;

    void* buffer = calloc(BUFFERSIZE, sizeof(char));

    DWORD bytesRead = 0;
    do
    {
        void* buffer = calloc(BUFFERSIZE, sizeof(char));
        ReadFile(handle, buffer, BUFFERSIZE - 1, &bytesRead, NULL);
        ret.append(reinterpret_cast<const char*>(buffer), bytesRead);
    } while (bytesRead == BUFFERSIZE - 1);

    free(buffer);

    return ret;
}

std::vector<std::string_view> lineSplit(const std::string_view& str)
{
    constexpr char delimiter = '\n';
    std::vector<std::string_view> results;
    size_t startIndex = 0;
    size_t delimiterIndex = str.find(delimiter, startIndex);
    while (delimiterIndex != std::string_view::npos)
    {
        const size_t endIndex = ((delimiterIndex > 0) && (str[delimiterIndex - 1] == '\r')) ? (delimiterIndex - 1) : delimiterIndex;
        results.emplace_back(str.substr(startIndex, endIndex - startIndex));
        startIndex = delimiterIndex + 1;
        delimiterIndex = str.find(delimiter, startIndex);
    }

    results.emplace_back(str.substr(startIndex));
    return results;
}

/*
std::string_view trimString(const std::string_view& str)
{
    const auto start = str.find_first_not_of(' ');
    const auto end = str.find_last_not_of(' ');
    return str.substr(start, end - start);
}*/

std::optional<std::pair<std::string_view, std::string_view>> splitKV(const std::string_view& str)
{
    constexpr char delimiter = ' ';
    const size_t delimiterIndex = str.find(delimiter);
    if (delimiterIndex != std::string_view::npos)
    {
        const auto value = str.substr(delimiterIndex + 1, str.size() - (delimiterIndex + 1));
        return { { str.substr(0, delimiterIndex), (value) } };
    }

    return std::nullopt;

}

ConstantReader::Constant ConstantReader::ParseConstant(const std::string_view& str)
{
    if (stringEqualIgnoreCase(str, "true"))
    {
        return { true };
    }

    if (stringEqualIgnoreCase(str, "false"))
    {
        return { false };
    }

    double doubleVal;
    auto[p, ec] = std::from_chars(str.data(), str.data() + str.size(), doubleVal);
    if (ec == std::errc())
    {
        return { doubleVal };
    }

    // Could parse as anything
    // Interpret as string
    return { std::string(str) };
}

std::unordered_map<std::string_view, ConstantReader::Constant> ConstantReader::ParseValues(const std::string_view& str)
{
    std::unordered_map<std::string_view, ConstantReader::Constant> map;

    const auto lines = lineSplit(str);
    for (const auto& line : lines)
    {
        const auto kv = splitKV(line);
        if (kv.has_value())
        {
            const auto value = ConstantReader::ParseConstant(kv->second);
            map.insert({ kv->first, value });
        }
    }

    return map;
}

ConstantReader::ConstantReader(const std::string_view& path)
{
    m_handle = CreateFileA(path.data(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (m_handle == INVALID_HANDLE_VALUE)
    {
        throw std::runtime_error("Could not load constant file.");
    }

    FILETIME creationTime;
    FILETIME lastAccessTime;
    FILETIME lastWriteTime;
    GetFileTime(m_handle, &creationTime, &lastAccessTime, &lastWriteTime);
    m_lastModified = lastWriteTime;
    m_fileContents = ReadAll(m_handle);
    m_values = this->ParseValues(m_fileContents);
}

ConstantReader::~ConstantReader()
{
    CloseHandle(m_handle);
}

void ConstantReader::Refresh()
{
    const std::unique_lock<std::shared_mutex> guard(m_rwMutex);

    FILETIME newLastModifiedTime;
    if (this->ShouldRebuild(&newLastModifiedTime))
    {
        const auto data = ReadAll(m_handle);
        if (data.size() > 0)
        {
            m_fileContents = data;
            m_values = this->ParseValues(m_fileContents);
            m_lastModified = newLastModifiedTime;
        }
    }
}

bool ConstantReader::ShouldRebuild(LPFILETIME lastWriteTime) const
{
    FILETIME creationTime;
    FILETIME lastAccessTime;
    GetFileTime(m_handle, &creationTime, &lastAccessTime, lastWriteTime);

    return (lastWriteTime->dwHighDateTime != m_lastModified.dwHighDateTime || lastWriteTime->dwLowDateTime != m_lastModified.dwLowDateTime);
}

int ConstantReader::GetInt(const std::string_view& name) const
{
    return static_cast<int>(this->GetDouble(name));
}

uint32_t ConstantReader::GetUint(const std::string_view& name) const
{
    return static_cast<uint32_t>(this->GetDouble(name));
}

bool ConstantReader::GetBool(const std::string_view& val) const
{
    const auto x = this->FindConstant(val);

    if (x.has_value() && std::holds_alternative<bool>(*x))
    {
        return std::get<bool>(*x);
    }

    return false;
}

double ConstantReader::GetDouble(const std::string_view& val) const
{
    const auto x = this->FindConstant(val);

    if (x.has_value() && std::holds_alternative<double>(*x))
    {
        return std::get<double>(*x);
    }

    return 0.0;
}

std::string ConstantReader::GetString(const std::string_view& val) const
{
    const auto x = this->FindConstant(val);

    if (x.has_value() && std::holds_alternative<std::string>(*x))
    {
        return std::get<std::string>(*x);
    }

    return "";
}

std::optional<ConstantReader::Constant> ConstantReader::FindConstant(const std::string_view& name) const
{
    const std::shared_lock<std::shared_mutex> guard(m_rwMutex);

    const auto existing = m_values.find(name);
    if (existing != m_values.end())
    {
        return existing->second;
    }

    return std::nullopt;
}