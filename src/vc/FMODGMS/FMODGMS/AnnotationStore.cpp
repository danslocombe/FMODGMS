#include "AnnotationStore.h"
#include <charconv>

std::optional<TimeRange> TimeRange::Parse(const std::string_view& str)
{
    // Parse string
    // Form:
    //    (1.0, 1.7)

    const auto tupleStart = str.find('(');
    const auto separatorPos = str.find(',', tupleStart + 1);
    const auto secondStart = str.find_first_not_of(' ', separatorPos + 1);
    const auto tupleEnd = str.find(')', separatorPos + 1);

    if (tupleStart == str.npos || separatorPos == str.npos || secondStart == str.npos || tupleEnd == str.npos)
    {
        return {};
    }

    double first;
    auto[p, ec] = std::from_chars(str.data() + tupleStart + 1, str.data() + separatorPos, first);

    if (ec != std::errc())
    {
        return {};
    }

    double second;
    auto[p1, ec1] = std::from_chars(str.data() + secondStart, str.data() + tupleEnd, second);

    if (ec1 != std::errc())
    {
        return {};
    }

    return TimeRange{ first, second };
}

std::optional<Annotation> Annotation::Parse(const std::string_view& str)
{
    // Parse string
    // Form:
    //    "banana" (1.0, 1.7)

    const auto valueStart = str.find('\'');
    const auto valueEnd = str.find('\'', valueStart + 1);

    if (valueStart == str.npos || valueEnd == str.npos)
    {
        return {};
    }

    const auto value = str.substr(valueStart + 1, valueEnd - valueStart - 1);

    const auto timeRangeStr = str.substr(valueEnd + 1);
    const auto parsedTimeRange = TimeRange::Parse(timeRangeStr);

    if (!parsedTimeRange.has_value())
    {
        return {};
    }

    return Annotation{ std::string(value), parsedTimeRange.value() };
}

bool AnnotationStore::ParseAddAnnotationList(size_t soundId, const std::string_view& annotationList)
{
    std::string_view::size_type pos = 0;
    std::string_view::size_type end;

    do {
        end = annotationList.find(';', pos);
        const auto annotationStr = annotationList.substr(pos, end - pos);
        if (!this->ParseAddAnnotation(soundId, annotationStr))
        {
            return false;
        }
        pos = end + 1;
    } while (end != std::string_view::npos);

    return true;
}

bool AnnotationStore::ParseAddAnnotation(size_t soundId, const std::string_view& annotationStr)
{
    const auto annotation = Annotation::Parse(annotationStr);
    if (annotation.has_value())
    {
        this->AddAnnotation(soundId, annotation.value());
        return true;
    }

    return false;
}

void AnnotationStore::AddAnnotation(size_t soundId, Annotation annotation)
{
    std::unique_lock<std::shared_mutex> writeLock(m_rwLock);

    auto& entry = m_annotations[soundId];
    entry.emplace_back(std::move(annotation));
}

std::optional<std::string_view> AnnotationStore::GetAnnotation(size_t soundId, double position)
{
    std::shared_lock<std::shared_mutex> readLock(m_rwLock);

    const auto annotations = m_annotations.find(soundId);
    if (annotations != m_annotations.end())
    {
        const auto& annotationList = annotations->second;
        for (const auto& x : annotationList)
        {
            if (position > x.Range.Start && position < x.Range.End)
            {
                return x.Value;
            }
        }
    }

    return {};
}
