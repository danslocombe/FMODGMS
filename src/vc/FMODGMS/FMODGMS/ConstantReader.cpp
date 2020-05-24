#include "ConstantReader.h"
#include <handleapi.h> 
#include <fileapi.h> 
#include <optional>
#include <charconv>
//#include <timezoneapi.h>

//ConstantReader DCR::Globals("C:\\users\\daslocom\\tmp\\danconstants.txt");

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
    double doubleVal;
    auto[p, ec] = std::from_chars(str.data(), str.data() + str.size(), doubleVal);
    if (ec != std::errc())
    {
        // Could not parse as double
        // Return as string
        return { str };
    }

    return { doubleVal };
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
    std::unique_lock<std::shared_mutex> writeLock(m_rwLock);

    if (this->ShouldRebuild())
    {
        m_fileContents = ReadAll(m_handle);
        m_values = this->ParseValues(m_fileContents);
    }
}

bool ConstantReader::ShouldRebuild()
{
    FILETIME creationTime;
    FILETIME lastAccessTime;
    FILETIME lastWriteTime;
    GetFileTime(m_handle, &creationTime, &lastAccessTime, &lastWriteTime);

    if (lastWriteTime.dwHighDateTime != m_lastModified.dwHighDateTime || lastWriteTime.dwLowDateTime != m_lastModified.dwLowDateTime)
    {
        m_lastModified = lastWriteTime;
        return true;
    }

    return false;
}

int ConstantReader::GetInt(const std::string_view& name) const
{
    return static_cast<int>(this->GetDouble(name));
}

uint32_t ConstantReader::GetUint(const std::string_view& name) const
{
    return static_cast<uint32_t>(this->GetDouble(name));
}

double ConstantReader::GetDouble(const std::string_view& val) const
{
    std::shared_lock<std::shared_mutex> readLock(m_rwLock);

    const auto existing = m_values.find(val);
    if (existing != m_values.end())
    {
        const auto val = existing->second;
        if (std::holds_alternative<double>(val))
        {
            return std::get<double>(val);
        }
    }

    return 0.0;
}

const std::string_view& ConstantReader::GetString(const std::string_view& val) const
{
    std::shared_lock<std::shared_mutex> readLock(m_rwLock);

    const auto existing = m_values.find(val);
    if (existing != m_values.end())
    {
        const auto val = existing->second;
        if (std::holds_alternative<std::string_view>(val))
        {
            return std::get<std::string_view>(val);
        }
    }

    return "";
}