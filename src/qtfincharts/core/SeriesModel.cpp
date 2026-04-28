#include "SeriesModel.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace QtFinCharts {

namespace {

    SeriesType normalizeLineLikeType(SeriesType type)
    {
        if (type == SeriesType::Area || type == SeriesType::Baseline) {
            return type;
        }
        return SeriesType::Line;
    }

    template <typename Point>
    void sortAndCoalesce(QVector<Point>& points)
    {
        std::sort(points.begin(), points.end(), [](const Point& lhs, const Point& rhs) {
            return lhs.time < rhs.time;
        });

        auto out = points.begin();
        for (auto it = points.begin(); it != points.end(); ++it) {
            if (out != points.begin() && std::prev(out)->time == it->time) {
                *std::prev(out) = *it;
                continue;
            }
            *out++ = *it;
        }
        points.erase(out, points.end());
    }

    void sortAndCoalesceTimes(QVector<TimePoint>& times)
    {
        std::sort(times.begin(), times.end());
        times.erase(std::unique(times.begin(), times.end()), times.end());
    }

    void sortAndCoalesceDataRows(QVector<SeriesDataRow>& rows)
    {
        std::stable_sort(rows.begin(), rows.end(), [](const SeriesDataRow& lhs, const SeriesDataRow& rhs) {
            return lhs.time < rhs.time;
        });

        auto out = rows.begin();
        for (auto it = rows.begin(); it != rows.end(); ++it) {
            if (out != rows.begin() && std::prev(out)->time == it->time) {
                *std::prev(out) = *it;
                continue;
            }
            *out++ = *it;
        }
        rows.erase(out, rows.end());
    }

    template <typename Point>
    void removeWhitespaceOverlappingData(QVector<TimePoint>& whitespaceTimes, const QVector<Point>& points)
    {
        if (whitespaceTimes.isEmpty() || points.isEmpty()) {
            return;
        }

        auto out = whitespaceTimes.begin();
        for (auto it = whitespaceTimes.begin(); it != whitespaceTimes.end(); ++it) {
            const auto pointIt = std::lower_bound(points.cbegin(), points.cend(), *it, [](const Point& point, TimePoint time) {
                return point.time < time;
            });
            if (pointIt != points.cend() && pointIt->time == *it) {
                continue;
            }
            *out++ = *it;
        }
        whitespaceTimes.erase(out, whitespaceTimes.end());
    }

    bool shouldConflate(qreal barSpacing, qreal threshold) noexcept
    {
        return std::isfinite(barSpacing)
            && std::isfinite(threshold)
            && barSpacing > 0.0
            && threshold > 0.0
            && barSpacing < threshold;
    }

    int pointIndex(const QHash<TimePoint, int>& indexByTime, TimePoint time)
    {
        const auto it = indexByTime.constFind(time);
        return it == indexByTime.constEnd() ? -1 : it.value();
    }

    int conflationBucket(int logicalIndex, qreal barSpacing, qreal threshold)
    {
        return static_cast<int>(std::floor(static_cast<qreal>(logicalIndex) * barSpacing / threshold));
    }

} // namespace

SeriesModel::SeriesModel(SeriesType type)
    : m_type(type)
{
}

void SeriesModel::setCandlesticks(QVector<CandlestickDataPoint> points)
{
    setOhlcPoints(std::move(points), SeriesType::Candlestick);
}

void SeriesModel::setOhlcPoints(QVector<CandlestickDataPoint> points,
    SeriesType type,
    QVector<TimePoint> whitespaceTimes)
{
    m_type = type == SeriesType::Bar ? SeriesType::Bar : SeriesType::Candlestick;
    sortAndCoalesce(points);
    sortAndCoalesceTimes(whitespaceTimes);
    removeWhitespaceOverlappingData(whitespaceTimes, points);
    m_candlesticks = std::move(points);
    m_whitespaceTimes = std::move(whitespaceTimes);
    m_linePoints.clear();
    m_histogramPoints.clear();
    m_originalDataRows.clear();
}

void SeriesModel::setLinePoints(QVector<LineDataPoint> points,
    SeriesType type,
    QVector<TimePoint> whitespaceTimes)
{
    m_type = normalizeLineLikeType(type);
    sortAndCoalesce(points);
    sortAndCoalesceTimes(whitespaceTimes);
    removeWhitespaceOverlappingData(whitespaceTimes, points);
    m_linePoints = std::move(points);
    m_whitespaceTimes = std::move(whitespaceTimes);
    m_candlesticks.clear();
    m_histogramPoints.clear();
    m_originalDataRows.clear();
}

void SeriesModel::setHistogramPoints(QVector<HistogramDataPoint> points, QVector<TimePoint> whitespaceTimes)
{
    m_type = SeriesType::Histogram;
    sortAndCoalesce(points);
    sortAndCoalesceTimes(whitespaceTimes);
    removeWhitespaceOverlappingData(whitespaceTimes, points);
    m_histogramPoints = std::move(points);
    m_whitespaceTimes = std::move(whitespaceTimes);
    m_candlesticks.clear();
    m_linePoints.clear();
    m_originalDataRows.clear();
}

void SeriesModel::setBaselineBaseValue(double baseValue)
{
    if (std::isfinite(baseValue)) {
        m_baselineBaseValue = baseValue;
    }
}

void SeriesModel::setHistogramBaseValue(double baseValue)
{
    if (std::isfinite(baseValue)) {
        m_histogramBaseValue = baseValue;
    }
}

void SeriesModel::setOriginalDataRows(QVector<SeriesDataRow> rows)
{
    sortAndCoalesceDataRows(rows);
    m_originalDataRows = std::move(rows);
}

bool SeriesModel::updateCandlestick(const CandlestickDataPoint& point)
{
    return updateOhlcPoint(point, SeriesType::Candlestick);
}

bool SeriesModel::updateOhlcPoint(const CandlestickDataPoint& point, SeriesType type)
{
    m_type = type == SeriesType::Bar ? SeriesType::Bar : SeriesType::Candlestick;
    m_linePoints.clear();
    m_histogramPoints.clear();
    removeTime(m_whitespaceTimes, point.time);
    return updateSorted(m_candlesticks, point);
}

bool SeriesModel::updateLinePoint(const LineDataPoint& point, SeriesType type)
{
    m_type = normalizeLineLikeType(type);
    m_candlesticks.clear();
    m_histogramPoints.clear();
    removeTime(m_whitespaceTimes, point.time);
    return updateSorted(m_linePoints, point);
}

bool SeriesModel::updateHistogramPoint(const HistogramDataPoint& point)
{
    m_type = SeriesType::Histogram;
    m_candlesticks.clear();
    m_linePoints.clear();
    removeTime(m_whitespaceTimes, point.time);
    return updateSorted(m_histogramPoints, point);
}

bool SeriesModel::updateWhitespace(TimePoint time, SeriesType type)
{
    if (type == SeriesType::Bar) {
        m_type = SeriesType::Bar;
    } else if (type == SeriesType::Candlestick || type == SeriesType::Histogram) {
        m_type = type;
    } else {
        m_type = normalizeLineLikeType(type);
    }

    switch (m_type) {
    case SeriesType::Candlestick:
    case SeriesType::Bar:
        removeTime(m_candlesticks, time);
        m_linePoints.clear();
        m_histogramPoints.clear();
        break;
    case SeriesType::Line:
    case SeriesType::Area:
    case SeriesType::Baseline:
        removeTime(m_linePoints, time);
        m_candlesticks.clear();
        m_histogramPoints.clear();
        break;
    case SeriesType::Histogram:
        removeTime(m_histogramPoints, time);
        m_candlesticks.clear();
        m_linePoints.clear();
        break;
    }

    return updateSortedTime(m_whitespaceTimes, time);
}

bool SeriesModel::updateOriginalDataRow(const SeriesDataRow& row)
{
    return updateSorted(m_originalDataRows, row);
}

int SeriesModel::pop(int count)
{
    if (count <= 0) {
        return 0;
    }

    int removed = 0;
    while (removed < count) {
        const std::optional<TimePoint> latest = lastTime();
        if (!latest.has_value()) {
            break;
        }

        bool didRemove = removeTime(m_whitespaceTimes, latest.value());
        didRemove = removeTime(m_originalDataRows, latest.value()) || didRemove;
        switch (m_type) {
        case SeriesType::Candlestick:
        case SeriesType::Bar:
            didRemove = removeTime(m_candlesticks, latest.value()) || didRemove;
            break;
        case SeriesType::Line:
        case SeriesType::Area:
        case SeriesType::Baseline:
            didRemove = removeTime(m_linePoints, latest.value()) || didRemove;
            break;
        case SeriesType::Histogram:
            didRemove = removeTime(m_histogramPoints, latest.value()) || didRemove;
            break;
        }

        if (!didRemove) {
            break;
        }
        ++removed;
    }

    return removed;
}

QVector<TimePoint> SeriesModel::times() const
{
    QVector<TimePoint> result;
    qsizetype reserve = m_whitespaceTimes.size();
    switch (m_type) {
    case SeriesType::Candlestick:
    case SeriesType::Bar:
        reserve += m_candlesticks.size();
        break;
    case SeriesType::Line:
    case SeriesType::Area:
    case SeriesType::Baseline:
        reserve += m_linePoints.size();
        break;
    case SeriesType::Histogram:
        reserve += m_histogramPoints.size();
        break;
    }
    result.reserve(reserve);
    result += m_whitespaceTimes;

    if (m_type == SeriesType::Candlestick || m_type == SeriesType::Bar) {
        for (const CandlestickDataPoint& point : m_candlesticks) {
            result.push_back(point.time);
        }
    } else if (m_type == SeriesType::Histogram) {
        for (const HistogramDataPoint& point : m_histogramPoints) {
            result.push_back(point.time);
        }
    } else {
        for (const LineDataPoint& point : m_linePoints) {
            result.push_back(point.time);
        }
    }
    std::sort(result.begin(), result.end());
    result.erase(std::unique(result.begin(), result.end()), result.end());
    return result;
}

QVector<CandlestickDataPoint> SeriesModel::conflatedOhlcPoints(const QHash<TimePoint, int>& indexByTime,
    int from,
    int to,
    qreal barSpacing,
    qreal threshold) const
{
    QVector<CandlestickDataPoint> result;
    if (from > to || m_candlesticks.isEmpty()) {
        return result;
    }

    const bool conflate = shouldConflate(barSpacing, threshold);
    result.reserve(conflate ? std::min(m_candlesticks.size(), static_cast<qsizetype>((to - from) + 1)) : m_candlesticks.size());

    CandlestickDataPoint aggregate;
    bool hasAggregate = false;
    int currentBucket = std::numeric_limits<int>::min();
    auto flush = [&] {
        if (!hasAggregate) {
            return;
        }
        result.push_back(aggregate);
        hasAggregate = false;
    };

    for (const CandlestickDataPoint& point : m_candlesticks) {
        const int index = pointIndex(indexByTime, point.time);
        if (index < 0 || index < from || index > to) {
            continue;
        }

        if (!conflate) {
            result.push_back(point);
            continue;
        }

        const int bucket = conflationBucket(index, barSpacing, threshold);
        if (!hasAggregate || bucket != currentBucket) {
            flush();
            aggregate = point;
            currentBucket = bucket;
            hasAggregate = true;
            continue;
        }

        aggregate.high = std::max(aggregate.high, point.high);
        aggregate.low = std::min(aggregate.low, point.low);
        aggregate.close = point.close;
        aggregate.time = point.time;
        aggregate.color = point.color;
        aggregate.borderColor = point.borderColor;
        aggregate.wickColor = point.wickColor;
    }
    flush();
    return result;
}

QVector<LineDataPoint> SeriesModel::conflatedLinePoints(const QHash<TimePoint, int>& indexByTime,
    int from,
    int to,
    qreal barSpacing,
    qreal threshold) const
{
    QVector<LineDataPoint> result;
    if (from > to || m_linePoints.isEmpty()) {
        return result;
    }

    const bool conflate = shouldConflate(barSpacing, threshold);
    result.reserve(conflate ? std::min(m_linePoints.size(), static_cast<qsizetype>((to - from) + 1)) : m_linePoints.size());

    struct Bucket {
        LineDataPoint first;
        LineDataPoint minimum;
        LineDataPoint maximum;
        LineDataPoint last;
        int firstIndex = -1;
        int minimumIndex = -1;
        int maximumIndex = -1;
        int lastIndex = -1;
        bool hasValue = false;
    };

    Bucket bucketState;
    int currentBucket = std::numeric_limits<int>::min();
    auto flush = [&] {
        if (!bucketState.hasValue) {
            return;
        }

        QVector<std::pair<int, LineDataPoint>> candidates {
            { bucketState.firstIndex, bucketState.first },
            { bucketState.minimumIndex, bucketState.minimum },
            { bucketState.maximumIndex, bucketState.maximum },
            { bucketState.lastIndex, bucketState.last },
        };
        std::sort(candidates.begin(), candidates.end(), [](const auto& lhs, const auto& rhs) {
            return lhs.first < rhs.first;
        });

        int lastIndex = std::numeric_limits<int>::min();
        for (const auto& candidate : candidates) {
            if (candidate.first == lastIndex) {
                continue;
            }
            result.push_back(candidate.second);
            lastIndex = candidate.first;
        }
        bucketState = { };
    };

    for (const LineDataPoint& point : m_linePoints) {
        const int index = pointIndex(indexByTime, point.time);
        if (index < 0 || index < from || index > to) {
            continue;
        }

        if (!conflate) {
            result.push_back(point);
            continue;
        }

        const int bucket = conflationBucket(index, barSpacing, threshold);
        if (!bucketState.hasValue || bucket != currentBucket) {
            flush();
            currentBucket = bucket;
            bucketState.first = point;
            bucketState.minimum = point;
            bucketState.maximum = point;
            bucketState.last = point;
            bucketState.firstIndex = index;
            bucketState.minimumIndex = index;
            bucketState.maximumIndex = index;
            bucketState.lastIndex = index;
            bucketState.hasValue = true;
            continue;
        }

        if (point.value < bucketState.minimum.value) {
            bucketState.minimum = point;
            bucketState.minimumIndex = index;
        }
        if (point.value > bucketState.maximum.value) {
            bucketState.maximum = point;
            bucketState.maximumIndex = index;
        }
        bucketState.last = point;
        bucketState.lastIndex = index;
    }
    flush();
    return result;
}

QVector<HistogramDataPoint> SeriesModel::conflatedHistogramPoints(const QHash<TimePoint, int>& indexByTime,
    int from,
    int to,
    qreal barSpacing,
    qreal threshold) const
{
    QVector<HistogramDataPoint> result;
    if (from > to || m_histogramPoints.isEmpty()) {
        return result;
    }

    const bool conflate = shouldConflate(barSpacing, threshold);
    result.reserve(conflate ? std::min(m_histogramPoints.size(), static_cast<qsizetype>((to - from) + 1)) : m_histogramPoints.size());

    HistogramDataPoint representative;
    bool hasRepresentative = false;
    int currentBucket = std::numeric_limits<int>::min();
    auto flush = [&] {
        if (!hasRepresentative) {
            return;
        }
        result.push_back(representative);
        hasRepresentative = false;
    };

    for (const HistogramDataPoint& point : m_histogramPoints) {
        const int index = pointIndex(indexByTime, point.time);
        if (index < 0 || index < from || index > to) {
            continue;
        }

        if (!conflate) {
            result.push_back(point);
            continue;
        }

        const int bucket = conflationBucket(index, barSpacing, threshold);
        if (!hasRepresentative || bucket != currentBucket) {
            flush();
            representative = point;
            currentBucket = bucket;
            hasRepresentative = true;
            continue;
        }

        if (std::abs(point.value - m_histogramBaseValue) >= std::abs(representative.value - m_histogramBaseValue)) {
            representative = point;
        }
    }
    flush();
    return result;
}

std::optional<TimePoint> SeriesModel::lastTime() const
{
    std::optional<TimePoint> latest;
    auto include = [&latest](TimePoint time) {
        if (!latest.has_value() || time > latest.value()) {
            latest = time;
        }
    };

    if (!m_whitespaceTimes.isEmpty()) {
        include(m_whitespaceTimes.last());
    }

    switch (m_type) {
    case SeriesType::Candlestick:
    case SeriesType::Bar:
        if (!m_candlesticks.isEmpty()) {
            include(m_candlesticks.last().time);
        }
        break;
    case SeriesType::Line:
    case SeriesType::Area:
    case SeriesType::Baseline:
        if (!m_linePoints.isEmpty()) {
            include(m_linePoints.last().time);
        }
        break;
    case SeriesType::Histogram:
        if (!m_histogramPoints.isEmpty()) {
            include(m_histogramPoints.last().time);
        }
        break;
    }

    return latest;
}

PriceRange SeriesModel::priceRangeForLogicalRange(const QHash<TimePoint, int>& indexByTime,
    int from,
    int to,
    PriceScaleMode priceScaleMode) const
{
    PriceRange range;
    if (from > to) {
        return range;
    }

    const bool logarithmic = priceScaleMode == PriceScaleMode::Logarithmic;
    auto includePrice = [&range, logarithmic](double price) {
        if (logarithmic && price <= 0.0) {
            return;
        }
        range.include(price);
    };

    if (m_type == SeriesType::Candlestick || m_type == SeriesType::Bar) {
        for (const CandlestickDataPoint& point : m_candlesticks) {
            const auto indexIt = indexByTime.constFind(point.time);
            if (indexIt == indexByTime.constEnd() || indexIt.value() < from || indexIt.value() > to) {
                continue;
            }
            if (logarithmic) {
                includePrice(point.open);
                includePrice(point.high);
                includePrice(point.low);
                includePrice(point.close);
            } else {
                includePrice(point.low);
                includePrice(point.high);
            }
        }
    } else if (m_type == SeriesType::Histogram) {
        includePrice(m_histogramBaseValue);
        for (const HistogramDataPoint& point : m_histogramPoints) {
            const auto indexIt = indexByTime.constFind(point.time);
            if (indexIt == indexByTime.constEnd() || indexIt.value() < from || indexIt.value() > to) {
                continue;
            }
            includePrice(point.value);
        }
    } else {
        if (m_type == SeriesType::Baseline) {
            includePrice(m_baselineBaseValue);
        }
        for (const LineDataPoint& point : m_linePoints) {
            const auto indexIt = indexByTime.constFind(point.time);
            if (indexIt == indexByTime.constEnd() || indexIt.value() < from || indexIt.value() > to) {
                continue;
            }
            includePrice(point.value);
        }
    }

    return range;
}

std::optional<double> SeriesModel::priceAt(TimePoint time, MarkerPosition position) const
{
    if (m_type == SeriesType::Candlestick || m_type == SeriesType::Bar) {
        const auto it = std::lower_bound(m_candlesticks.cbegin(), m_candlesticks.cend(), time, [](const CandlestickDataPoint& point, TimePoint value) {
            return point.time < value;
        });
        if (it == m_candlesticks.cend() || it->time != time) {
            return std::nullopt;
        }
        switch (position) {
        case MarkerPosition::AboveBar:
            return it->high;
        case MarkerPosition::BelowBar:
            return it->low;
        case MarkerPosition::InBar:
            return it->close;
        case MarkerPosition::AtPriceTop:
        case MarkerPosition::AtPriceMiddle:
        case MarkerPosition::AtPriceBottom:
            return std::nullopt;
        }
    }

    if (m_type == SeriesType::Histogram) {
        const auto it = std::lower_bound(m_histogramPoints.cbegin(), m_histogramPoints.cend(), time, [](const HistogramDataPoint& point, TimePoint value) {
            return point.time < value;
        });
        if (it == m_histogramPoints.cend() || it->time != time) {
            return std::nullopt;
        }
        return it->value;
    }

    const auto it = std::lower_bound(m_linePoints.cbegin(), m_linePoints.cend(), time, [](const LineDataPoint& point, TimePoint value) {
        return point.time < value;
    });
    if (it == m_linePoints.cend() || it->time != time) {
        return std::nullopt;
    }
    return it->value;
}

void SeriesModel::setMarkers(QVector<SeriesMarker> markers)
{
    std::sort(markers.begin(), markers.end(), [](const SeriesMarker& lhs, const SeriesMarker& rhs) {
        return lhs.time < rhs.time;
    });
    m_markers = std::move(markers);
}

template <typename Point>
bool SeriesModel::updateSorted(QVector<Point>& points, const Point& point)
{
    const auto it = std::lower_bound(points.begin(), points.end(), point.time, [](const Point& lhs, TimePoint time) {
        return lhs.time < time;
    });
    if (it != points.end() && it->time == point.time) {
        *it = point;
        return false;
    }

    points.insert(it, point);
    return true;
}

bool SeriesModel::updateSortedTime(QVector<TimePoint>& times, TimePoint time)
{
    const auto it = std::lower_bound(times.begin(), times.end(), time);
    if (it != times.end() && *it == time) {
        return false;
    }

    times.insert(it, time);
    return true;
}

template <typename Point>
bool SeriesModel::removeTime(QVector<Point>& points, TimePoint time)
{
    const auto it = std::lower_bound(points.begin(), points.end(), time, [](const Point& point, TimePoint value) {
        return point.time < value;
    });
    if (it == points.end() || it->time != time) {
        return false;
    }

    points.erase(it);
    return true;
}

bool SeriesModel::removeTime(QVector<TimePoint>& times, TimePoint time)
{
    const auto it = std::lower_bound(times.begin(), times.end(), time);
    if (it == times.end() || *it != time) {
        return false;
    }

    times.erase(it);
    return true;
}

} // namespace QtFinCharts
