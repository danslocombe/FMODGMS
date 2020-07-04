#include "ConstantReader.h"
#include <handleapi.h> 
#include <fileapi.h> 
#include <optional>
#include <charconv>
#include "StringHelpers.h"

typedef _ConstantReader_Constant Constant;

ConstantObj::ConstantObj(std::unordered_map<std::string_view, Constant> values) : m_values(std::move(values))
{}

int ConstantObj::GetInt(const std::string_view& name) const
{
    return static_cast<int>(this->GetDouble(name));
}

uint32_t ConstantObj::GetUint(const std::string_view& name) const
{
    return static_cast<uint32_t>(this->GetDouble(name));
}

bool ConstantObj::GetBool(const std::string_view& val) const
{
    const auto x = this->FindConstant(val);

    if (x.has_value() && std::holds_alternative<bool>(*x))
    {
        return std::get<bool>(*x);
    }

    return false;
}

double ConstantObj::GetDouble(const std::string_view& val) const
{
    const auto x = this->FindConstant(val);

    if (x.has_value() && std::holds_alternative<double>(*x))
    {
        return std::get<double>(*x);
    }

    return 0.0;
}

std::string ConstantObj::GetString(const std::string_view& val) const
{
    const auto x = this->FindConstant(val);

    if (x.has_value() && std::holds_alternative<std::string>(*x))
    {
        return std::get<std::string>(*x);
    }

    return "";
}

std::shared_ptr<const ConstantObj> ConstantObj::GetObj(const std::string_view& name) const
{
    const auto x = this->FindConstant(name);

    if (x.has_value() && std::holds_alternative<std::shared_ptr<const ConstantObj>>(*x))
    {
        return std::get<std::shared_ptr<const ConstantObj>>(*x);
    }

    return nullptr;
}

std::optional<Constant> ConstantObj::FindConstant(const std::string_view& name) const
{
    const auto existing = m_values.find(name);
    if (existing != m_values.end())
    {
        return existing->second;
    }

    return std::nullopt;
}


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

std::string_view trimString(const std::string_view& str)
{
    const auto start = str.find_first_not_of(' ');
    if (start == std::string_view::npos)
    {
        // Whitespace or empty string
        return str;
    }

    const auto end = str.find_last_not_of(' ');
    return str.substr(start, (end + 1) - start);
}

std::optional<std::pair<std::string_view, std::string_view>> splitKV(const std::string_view& str)
{
    const auto trimmed = trimString(str);
    constexpr char delimiter = ' ';
    const size_t delimiterIndex = trimmed.find(delimiter);
    if (delimiterIndex != std::string_view::npos)
    {
        const auto value = trimmed.substr(delimiterIndex + 1, trimmed.size() - (delimiterIndex + 1));
        return { { trimmed.substr(0, delimiterIndex), (value) } };
    }

    return std::nullopt;

}

Constant ConstantReader::ParseConstant(const std::string_view& str)
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

std::optional<std::pair<std::string_view, Constant>> ConstantReader::ParseObject(uint32_t& i, const std::vector<std::string_view>& lines)
{
    const auto& line = lines[i];
    const auto kv = splitKV(line);
    i++;

    const bool nextLineOpeningBrace = i < lines.size() && lines[i] == "{";

    if (kv.has_value() || nextLineOpeningBrace)
    {
        // Todo strip whitespace in {} checks
        // Warning kv.value() may not be defined
        if (nextLineOpeningBrace || kv->second == "{")
        {
            // Define an object
            std::string_view objectName;

            if (nextLineOpeningBrace)
            {
                // Skip over this line in next call
                i++;

                objectName = line;
            }
            else
            {
                objectName = kv->first;
            }

            std::unordered_map<std::string_view, Constant> fields;
            while (i < lines.size() && lines[i] != "}")
            {
                const auto next = ParseObject(i, lines);
                if (next.has_value())
                {
                    fields[next.value().first] = next.value().second;
                }
            }

            auto co = std::make_shared<const ConstantObj>(std::move(fields));
            return std::make_optional(std::make_pair(objectName, std::move(co)));
        }
        else
        {
            // We know kv is defined
            const auto value = ConstantReader::ParseConstant(kv->second);
            return std::make_optional(std::make_pair(kv->first, value));
        }
    }

    return std::nullopt;
}

std::unordered_map<std::string_view, Constant> ConstantReader::ParseValues(const std::string_view& str)
{
    std::unordered_map<std::string_view, Constant> map;
    const auto lines = lineSplit(str);
    uint32_t i = 0;

    while (i < lines.size())
    {
        const auto object = ParseObject(i, lines);
        if (object.has_value())
        {
            map.insert(object.value());
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

    constexpr bool forceRefresh = true;
    this->Refresh(forceRefresh);
}

ConstantReader::~ConstantReader()
{
    CloseHandle(m_handle);
}

void ConstantReader::Refresh(bool force)
{
    FILETIME newLastModifiedTime = m_lastModified;
    if (force || this->ShouldRebuild(&newLastModifiedTime))
    {
        const std::unique_lock<std::shared_mutex> guard(m_rwMutex);

        const auto data = ReadAll(m_handle);
        if (data.size() > 0)
        {
            m_fileContents = data;
            auto topLevelValues = this->ParseValues(m_fileContents);
            m_baseObj = ConstantObj(std::move(topLevelValues));
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
    const std::shared_lock<std::shared_mutex> guard(m_rwMutex);
    return static_cast<int>(this->GetDouble(name));
}

uint32_t ConstantReader::GetUint(const std::string_view& name) const
{
    const std::shared_lock<std::shared_mutex> guard(m_rwMutex);
    return static_cast<uint32_t>(this->GetDouble(name));
}

bool ConstantReader::GetBool(const std::string_view& val) const
{
    const std::shared_lock<std::shared_mutex> guard(m_rwMutex);
    return m_baseObj.GetBool(val);
}

double ConstantReader::GetDouble(const std::string_view& val) const
{
    const std::shared_lock<std::shared_mutex> guard(m_rwMutex);
    return m_baseObj.GetDouble(val);
}

std::string ConstantReader::GetString(const std::string_view& val) const
{
    const std::shared_lock<std::shared_mutex> guard(m_rwMutex);
    return m_baseObj.GetString(val);
}

std::shared_ptr<const ConstantObj> ConstantReader::GetObj(const std::string_view& val) const
{
    const std::shared_lock<std::shared_mutex> guard(m_rwMutex);
    return m_baseObj.GetObj(val);
}