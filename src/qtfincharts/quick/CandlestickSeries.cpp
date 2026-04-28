#include "CandlestickSeries.h"

#include <QtCore/QDebug>

namespace QtFinCharts {

namespace {

    bool isWhitespaceRow(const QVariantMap& row)
    {
        return row.contains(QStringLiteral("time"))
            && !row.contains(QStringLiteral("open"))
            && !row.contains(QStringLiteral("high"))
            && !row.contains(QStringLiteral("low"))
            && !row.contains(QStringLiteral("close"));
    }

} // namespace

CandlestickSeries::CandlestickSeries(QObject* parent)
    : ChartSeries(SeriesType::Candlestick, parent)
{
}

void CandlestickSeries::setUpColor(const QColor& color)
{
    if (m_upColor == color) {
        return;
    }
    m_upColor = color;
    notifyOptionsChanged();
}

void CandlestickSeries::setDownColor(const QColor& color)
{
    if (m_downColor == color) {
        return;
    }
    m_downColor = color;
    notifyOptionsChanged();
}

QColor CandlestickSeries::borderColor() const noexcept
{
    return m_borderUpColor == m_borderDownColor ? m_borderUpColor : QColor();
}

void CandlestickSeries::setBorderColor(const QColor& color)
{
    if (m_borderUpColor == color && m_borderDownColor == color) {
        return;
    }
    m_borderUpColor = color;
    m_borderDownColor = color;
    notifyOptionsChanged();
}

void CandlestickSeries::setBorderUpColor(const QColor& color)
{
    if (m_borderUpColor == color) {
        return;
    }
    m_borderUpColor = color;
    notifyOptionsChanged();
}

void CandlestickSeries::setBorderDownColor(const QColor& color)
{
    if (m_borderDownColor == color) {
        return;
    }
    m_borderDownColor = color;
    notifyOptionsChanged();
}

QColor CandlestickSeries::wickColor() const noexcept
{
    return m_wickUpColor == m_wickDownColor ? m_wickUpColor : QColor();
}

void CandlestickSeries::setWickColor(const QColor& color)
{
    if (m_wickUpColor == color && m_wickDownColor == color) {
        return;
    }
    m_wickUpColor = color;
    m_wickDownColor = color;
    notifyOptionsChanged();
}

void CandlestickSeries::setWickUpColor(const QColor& color)
{
    if (m_wickUpColor == color) {
        return;
    }
    m_wickUpColor = color;
    notifyOptionsChanged();
}

void CandlestickSeries::setWickDownColor(const QColor& color)
{
    if (m_wickDownColor == color) {
        return;
    }
    m_wickDownColor = color;
    notifyOptionsChanged();
}

void CandlestickSeries::setWickVisible(bool visible)
{
    if (m_wickVisible == visible) {
        return;
    }
    m_wickVisible = visible;
    notifyOptionsChanged();
}

void CandlestickSeries::setBorderVisible(bool visible)
{
    if (m_borderVisible == visible) {
        return;
    }
    m_borderVisible = visible;
    notifyOptionsChanged();
}

void CandlestickSeries::setData(const QVariantList& rows)
{
    QVector<CandlestickDataPoint> parsed;
    QVector<TimePoint> whitespaceTimes;
    QVector<SeriesDataRow> dataRows;
    parsed.reserve(rows.size());
    dataRows.reserve(rows.size());
    for (const QVariant& rowValue : rows) {
        const QVariantMap row = rowValue.toMap();
        CandlestickDataPoint point;
        if (parseRow(row, &point)) {
            parsed.push_back(point);
            dataRows.push_back({ point.time, row });
            continue;
        }

        TimePoint whitespaceTime = 0;
        if (isWhitespaceRow(row) && parseTime(row.value(QStringLiteral("time")), &whitespaceTime)) {
            whitespaceTimes.push_back(whitespaceTime);
            dataRows.push_back({ whitespaceTime, row });
            continue;
        }

        qWarning() << "QtFinCharts: skipped invalid candlestick row" << rowValue;
        continue;
    }

    model().setOhlcPoints(std::move(parsed), SeriesType::Candlestick, std::move(whitespaceTimes));
    model().setOriginalDataRows(std::move(dataRows));
    QVariantMap payload;
    payload.insert(QStringLiteral("count"), model().candlesticks().size());
    payload.insert(QStringLiteral("whitespaceCount"), model().whitespaceTimes().size());
    notifyDataChanged(QStringLiteral("full"), payload);
}

void CandlestickSeries::update(const QVariantMap& row, bool historicalUpdate)
{
    CandlestickDataPoint point;
    if (parseRow(row, &point)) {
        if (!shouldAcceptUpdate(point.time, historicalUpdate)) {
            return;
        }
        const bool inserted = model().updateCandlestick(point);
        model().updateOriginalDataRow({ point.time, row });
        QVariantMap payload;
        payload.insert(QStringLiteral("time"), point.time);
        payload.insert(QStringLiteral("inserted"), inserted);
        payload.insert(QStringLiteral("historicalUpdate"), historicalUpdate);
        notifyDataChanged(QStringLiteral("update"), payload);
        return;
    }

    TimePoint whitespaceTime = 0;
    if (!isWhitespaceRow(row) || !parseTime(row.value(QStringLiteral("time")), &whitespaceTime)) {
        qWarning() << "QtFinCharts: skipped invalid candlestick update" << row;
        return;
    }
    if (!shouldAcceptUpdate(whitespaceTime, historicalUpdate)) {
        return;
    }
    const bool inserted = model().updateWhitespace(whitespaceTime, SeriesType::Candlestick);
    model().updateOriginalDataRow({ whitespaceTime, row });
    QVariantMap payload;
    payload.insert(QStringLiteral("time"), whitespaceTime);
    payload.insert(QStringLiteral("inserted"), inserted);
    payload.insert(QStringLiteral("whitespace"), true);
    payload.insert(QStringLiteral("historicalUpdate"), historicalUpdate);
    notifyDataChanged(QStringLiteral("update"), payload);
}

bool CandlestickSeries::parseRow(const QVariantMap& row, CandlestickDataPoint* point)
{
    if (!point) {
        return false;
    }

    TimePoint time = 0;
    if (!parseTime(row.value(QStringLiteral("time")), &time)) {
        return false;
    }

    const std::optional<double> open = parseFiniteDouble(row.value(QStringLiteral("open")));
    const std::optional<double> high = parseFiniteDouble(row.value(QStringLiteral("high")));
    const std::optional<double> low = parseFiniteDouble(row.value(QStringLiteral("low")));
    const std::optional<double> close = parseFiniteDouble(row.value(QStringLiteral("close")));
    if (!open.has_value() || !high.has_value() || !low.has_value() || !close.has_value()) {
        return false;
    }

    *point = {
        time,
        open.value(),
        high.value(),
        low.value(),
        close.value(),
        colorFromVariant(row.value(QStringLiteral("color")), QColor()),
        colorFromVariant(row.value(QStringLiteral("borderColor")), QColor()),
        colorFromVariant(row.value(QStringLiteral("wickColor")), QColor()),
    };
    return true;
}

} // namespace QtFinCharts
