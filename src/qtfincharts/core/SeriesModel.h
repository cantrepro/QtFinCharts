#pragma once

#include "qtfincharts_export.h"

#include "Types.h"

#include <QtCore/QHash>
#include <QtCore/QVector>

#include <optional>

namespace QtFinCharts {

class QTFINCHARTS_EXPORT SeriesModel {
public:
    explicit SeriesModel(SeriesType type = SeriesType::Line);

    [[nodiscard]] SeriesType type() const noexcept { return m_type; }

    void setCandlesticks(QVector<CandlestickDataPoint> points);
    void setOhlcPoints(QVector<CandlestickDataPoint> points,
        SeriesType type,
        QVector<TimePoint> whitespaceTimes = { });
    void setLinePoints(QVector<LineDataPoint> points,
        SeriesType type = SeriesType::Line,
        QVector<TimePoint> whitespaceTimes = { });
    void setHistogramPoints(QVector<HistogramDataPoint> points, QVector<TimePoint> whitespaceTimes = { });
    void setBaselineBaseValue(double baseValue);
    void setHistogramBaseValue(double baseValue);
    void setOriginalDataRows(QVector<SeriesDataRow> rows);

    bool updateCandlestick(const CandlestickDataPoint& point);
    bool updateOhlcPoint(const CandlestickDataPoint& point, SeriesType type);
    bool updateLinePoint(const LineDataPoint& point, SeriesType type = SeriesType::Line);
    bool updateHistogramPoint(const HistogramDataPoint& point);
    bool updateWhitespace(TimePoint time, SeriesType type);
    bool updateOriginalDataRow(const SeriesDataRow& row);
    int pop(int count);

    [[nodiscard]] const QVector<CandlestickDataPoint>& candlesticks() const noexcept { return m_candlesticks; }
    [[nodiscard]] const QVector<LineDataPoint>& linePoints() const noexcept { return m_linePoints; }
    [[nodiscard]] const QVector<HistogramDataPoint>& histogramPoints() const noexcept { return m_histogramPoints; }
    [[nodiscard]] const QVector<TimePoint>& whitespaceTimes() const noexcept { return m_whitespaceTimes; }
    [[nodiscard]] double baselineBaseValue() const noexcept { return m_baselineBaseValue; }
    [[nodiscard]] double histogramBaseValue() const noexcept { return m_histogramBaseValue; }
    [[nodiscard]] const QVector<SeriesDataRow>& originalDataRows() const noexcept { return m_originalDataRows; }

    [[nodiscard]] QVector<TimePoint> times() const;
    [[nodiscard]] QVector<CandlestickDataPoint> conflatedOhlcPoints(const QHash<TimePoint, int>& indexByTime,
        int from,
        int to,
        qreal barSpacing,
        qreal threshold) const;
    [[nodiscard]] QVector<LineDataPoint> conflatedLinePoints(const QHash<TimePoint, int>& indexByTime,
        int from,
        int to,
        qreal barSpacing,
        qreal threshold) const;
    [[nodiscard]] QVector<HistogramDataPoint> conflatedHistogramPoints(const QHash<TimePoint, int>& indexByTime,
        int from,
        int to,
        qreal barSpacing,
        qreal threshold) const;
    [[nodiscard]] std::optional<TimePoint> lastTime() const;
    [[nodiscard]] PriceRange priceRangeForLogicalRange(const QHash<TimePoint, int>& indexByTime,
        int from,
        int to,
        PriceScaleMode priceScaleMode = PriceScaleMode::Normal) const;
    [[nodiscard]] std::optional<double> priceAt(TimePoint time, MarkerPosition position) const;

    void setMarkers(QVector<SeriesMarker> markers);
    [[nodiscard]] const QVector<SeriesMarker>& markers() const noexcept { return m_markers; }

private:
    template <typename Point>
    static bool updateSorted(QVector<Point>& points, const Point& point);
    static bool updateSortedTime(QVector<TimePoint>& times, TimePoint time);
    template <typename Point>
    static bool removeTime(QVector<Point>& points, TimePoint time);
    static bool removeTime(QVector<TimePoint>& times, TimePoint time);

    SeriesType m_type = SeriesType::Line;
    QVector<CandlestickDataPoint> m_candlesticks;
    QVector<LineDataPoint> m_linePoints;
    QVector<HistogramDataPoint> m_histogramPoints;
    QVector<TimePoint> m_whitespaceTimes;
    double m_baselineBaseValue = 0.0;
    double m_histogramBaseValue = 0.0;
    QVector<SeriesDataRow> m_originalDataRows;
    QVector<SeriesMarker> m_markers;
};

} // namespace QtFinCharts
