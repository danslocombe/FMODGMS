#pragma once
#include <string_view>
#include <unordered_map>
#include <optional>
#include <shared_mutex>

struct TimeRange
{
    static std::optional<TimeRange> Parse(const std::string_view& str);
    double Start;
    double End;
};

struct Annotation
{
    static std::optional<Annotation> Parse(const std::string_view& str);
    std::string Value;
    TimeRange Range;
};

class AnnotationStore
{
public:
    bool ParseAddAnnotationList(size_t soundId, const std::string_view& annotationList);
    bool ParseAddAnnotation(size_t soundId, const std::string_view& annotation);
    void AddAnnotation(size_t soundId, Annotation annotation);
    std::optional<std::string_view> GetAnnotation(size_t soundId, double position);
private:

    std::unordered_map<size_t, std::vector<Annotation>> m_annotations;
    std::shared_mutex m_rwLock;
};