#include "ChartModel.h"

#include <QtCore/QDateTime>
#include <QtCore/QTimeZone>

#include <algorithm>
#include <cmath>
#include <optional>

namespace QtFinCharts {

void ChartModel::setSeries(QVector<SeriesAttachment> series)
{
    m_series = std::move(series);
    rebuildTimeline();
}

void ChartModel::rebuildTimeline()
{
    const qsizetype oldPointCount = m_timePoints.size();
    const std::optional<TimePoint> oldLastTime = m_timePoints.isEmpty()
        ? std::nullopt
        : std::optional<TimePoint>(m_timePoints.last());

    QVector<TimePoint> merged;
    for (const SeriesAttachment& attachment : m_series) {
        if (!attachment.model || !attachment.visible) {
            continue;
        }
        const QVector<TimePoint> seriesTimes = attachment.model->times();
        merged += seriesTimes;
    }

    std::sort(merged.begin(), merged.end());
    merged.erase(std::unique(merged.begin(), merged.end()), merged.end());

    m_timePoints = std::move(merged);
    m_indexByTime.clear();
    m_indexByTime.reserve(m_timePoints.size());
    for (qsizetype i = 0; i < m_timePoints.size(); ++i) {
        m_indexByTime.insert(m_timePoints.at(i), static_cast<int>(i));
    }

    const bool newBarsAddedToRight = oldLastTime.has_value()
        && m_timePoints.size() > oldPointCount
        && !m_timePoints.isEmpty()
        && m_timePoints.last() > oldLastTime.value();
    m_timeScale.setPointCount(static_cast<int>(m_timePoints.size()), newBarsAddedToRight);
}

int ChartModel::indexForTime(TimePoint time) const
{
    const auto it = m_indexByTime.constFind(time);
    return it == m_indexByTime.constEnd() ? -1 : it.value();
}

TimePoint ChartModel::timeAt(int index) const
{
    if (index < 0 || index >= m_timePoints.size()) {
        return 0;
    }
    return m_timePoints.at(index);
}

void ChartModel::setPaneCount(int paneCount)
{
    m_paneCount = std::max(1, paneCount);
    ensurePaneSizing(m_paneCount);
    m_paneHeights.resize(m_paneCount);
    m_paneStretchFactors.resize(m_paneCount);
}

int ChartModel::effectivePaneCount() const noexcept
{
    int count = m_preserveEmptyPanes ? std::max(1, m_paneCount) : 1;
    for (const SeriesAttachment& attachment : m_series) {
        if (attachment.model && attachment.visible) {
            count = std::max(count, attachment.pane + 1);
        }
    }
    return count;
}

void ChartModel::setPaneHeight(int pane, qreal height)
{
    if (pane < 0) {
        return;
    }

    ensurePaneSizing(pane + 1);
    m_paneHeights[pane] = std::isfinite(height) && height > 0.0 ? height : 0.0;
}

void ChartModel::clearPaneHeight(int pane)
{
    setPaneHeight(pane, 0.0);
}

qreal ChartModel::paneHeight(int pane) const noexcept
{
    return pane >= 0 && pane < m_paneHeights.size() ? m_paneHeights.at(pane) : 0.0;
}

void ChartModel::setPaneStretchFactor(int pane, qreal factor)
{
    if (pane < 0) {
        return;
    }

    ensurePaneSizing(pane + 1);
    m_paneStretchFactors[pane] = std::isfinite(factor) ? std::clamp<qreal>(factor, 0.01, 100.0) : 1.0;
}

qreal ChartModel::paneStretchFactor(int pane) const noexcept
{
    if (pane < 0 || pane >= m_paneStretchFactors.size() || m_paneStretchFactors.at(pane) <= 0.0) {
        return 1.0;
    }
    return m_paneStretchFactors.at(pane);
}

void ChartModel::removePaneSizing(int pane)
{
    if (pane < 0) {
        return;
    }
    if (pane < m_paneHeights.size()) {
        m_paneHeights.removeAt(pane);
    }
    if (pane < m_paneStretchFactors.size()) {
        m_paneStretchFactors.removeAt(pane);
    }
    ensurePaneSizing(m_paneCount);
}

void ChartModel::swapPaneSizing(int first, int second)
{
    if (first < 0 || second < 0 || first == second) {
        return;
    }
    ensurePaneSizing(std::max(first, second) + 1);
    std::swap(m_paneHeights[first], m_paneHeights[second]);
    std::swap(m_paneStretchFactors[first], m_paneStretchFactors[second]);
}

ChartLayout ChartModel::layoutForSize(const QSizeF& size,
    bool timeScaleVisible,
    bool priceScaleVisible,
    qreal timeAxisHeight,
    qreal priceAxisMinimumWidth,
    bool leftPriceScaleVisible,
    qreal leftPriceAxisMinimumWidth) const
{
    ChartLayout layout;
    const qreal chartWidth = std::isfinite(size.width()) ? std::max<qreal>(0.0, size.width()) : 0.0;
    const qreal chartHeight = std::isfinite(size.height()) ? std::max<qreal>(0.0, size.height()) : 0.0;
    const qreal normalizedPriceAxisMinimumWidth = std::isfinite(priceAxisMinimumWidth)
        ? std::clamp<qreal>(priceAxisMinimumWidth, 0.0, 160.0)
        : 60.0;
    const qreal normalizedLeftPriceAxisMinimumWidth = std::isfinite(leftPriceAxisMinimumWidth)
        ? std::clamp<qreal>(leftPriceAxisMinimumWidth, 0.0, 160.0)
        : 60.0;
    const qreal rightPriceAxisWidth = priceScaleVisible
        ? std::clamp<qreal>(chartWidth * 0.16, normalizedPriceAxisMinimumWidth, std::max<qreal>(88.0, normalizedPriceAxisMinimumWidth))
        : 0.0;
    const qreal leftPriceAxisWidth = leftPriceScaleVisible
        ? std::clamp<qreal>(chartWidth * 0.16, normalizedLeftPriceAxisMinimumWidth, std::max<qreal>(88.0, normalizedLeftPriceAxisMinimumWidth))
        : 0.0;
    const qreal resolvedTimeAxisHeight = timeScaleVisible && std::isfinite(timeAxisHeight)
        ? std::clamp<qreal>(timeAxisHeight, 0.0, 96.0)
        : 0.0;
    const qreal separator = 1.0;
    const int paneCount = effectivePaneCount();
    const qreal dataLeft = leftPriceAxisWidth;
    const qreal dataWidth = std::max<qreal>(0.0, chartWidth - leftPriceAxisWidth - rightPriceAxisWidth);

    layout.leftPriceAxisWidth = leftPriceAxisWidth;
    layout.rightPriceAxisWidth = rightPriceAxisWidth;
    layout.priceAxisWidth = rightPriceAxisWidth;
    layout.timeAxisHeight = resolvedTimeAxisHeight;
    layout.timeAxisRect = QRectF(dataLeft,
        std::max<qreal>(0.0, chartHeight - resolvedTimeAxisHeight),
        dataWidth,
        resolvedTimeAxisHeight);

    const qreal totalPaneHeight = std::max<qreal>(0.0, chartHeight - resolvedTimeAxisHeight);
    const qreal availablePaneHeight = std::max<qreal>(0.0, totalPaneHeight - separator * (paneCount - 1));
    QVector<qreal> paneHeights(paneCount, 0.0);
    qreal fixedHeightSum = 0.0;
    qreal stretchSum = 0.0;
    for (int i = 0; i < paneCount; ++i) {
        const qreal fixedHeight = i < m_paneHeights.size() ? m_paneHeights.at(i) : 0.0;
        if (fixedHeight > 0.0) {
            paneHeights[i] = fixedHeight;
            fixedHeightSum += fixedHeight;
        } else {
            stretchSum += paneStretchFactor(i);
        }
    }

    if (fixedHeightSum >= availablePaneHeight && fixedHeightSum > 0.0) {
        const qreal scale = availablePaneHeight / fixedHeightSum;
        for (int i = 0; i < paneCount; ++i) {
            if (paneHeights.at(i) > 0.0) {
                paneHeights[i] *= scale;
            } else {
                paneHeights[i] = 0.0;
            }
        }
    } else {
        const qreal remainingHeight = std::max<qreal>(0.0, availablePaneHeight - fixedHeightSum);
        const qreal normalizedStretchSum = stretchSum > 0.0 ? stretchSum : static_cast<qreal>(paneCount);
        for (int i = 0; i < paneCount; ++i) {
            if (paneHeights.at(i) <= 0.0) {
                const qreal stretch = stretchSum > 0.0 ? paneStretchFactor(i) : 1.0;
                paneHeights[i] = remainingHeight * stretch / normalizedStretchSum;
            }
        }
    }

    layout.panes.reserve(paneCount);
    qreal y = 0.0;
    for (int i = 0; i < paneCount; ++i) {
        const qreal paneHeight = paneHeights.value(i, 0.0);
        ChartLayout::Pane pane;
        pane.index = i;
        pane.leftPriceAxisRect = QRectF(0.0, y, leftPriceAxisWidth, paneHeight);
        pane.dataRect = QRectF(dataLeft, y, dataWidth, paneHeight);
        pane.rightPriceAxisRect = QRectF(dataLeft + dataWidth, y, rightPriceAxisWidth, paneHeight);
        pane.priceAxisRect = pane.rightPriceAxisRect;
        layout.panes.push_back(pane);
        y += paneHeight + separator;
    }

    return layout;
}

PriceRange ChartModel::autoScaleRange(int pane, int from, int to, const QString& priceScaleId) const
{
    PriceRange range;
    for (const SeriesAttachment& attachment : m_series) {
        if (!attachment.model || !attachment.visible || attachment.pane != pane) {
            continue;
        }
        if (!priceScaleId.isEmpty() && attachment.priceScaleId != priceScaleId) {
            continue;
        }
        range.include(attachment.model->priceRangeForLogicalRange(m_indexByTime, from, to));
    }
    return range.padded(0.08);
}

QVector<TimeTick> ChartModel::buildTimeTicks(const QRectF& dataRect,
    const QLocale& locale,
    bool timeVisible,
    bool secondsVisible,
    int maxLabelWidth,
    const QString& labelFormat,
    bool uniformDistribution) const
{
    QVector<TimeTick> ticks;
    if (m_timePoints.isEmpty() || dataRect.width() <= 0.0) {
        return ticks;
    }

    const qreal spacing = std::max<qreal>(1.0, m_timeScale.barSpacing());
    const int normalizedMaxLabelWidth = std::clamp(maxLabelWidth, 24, 240);
    const int step = std::max(1, static_cast<int>(std::ceil(normalizedMaxLabelWidth / spacing)));
    const auto [visibleFrom, visibleTo] = m_timeScale.visibleIndexRange(step);
    if (visibleFrom > visibleTo) {
        return ticks;
    }

    const int first = uniformDistribution
        ? ((visibleFrom + step - 1) / step) * step
        : visibleFrom;
    ticks.reserve((visibleTo - first) / step + 1);

    const QString format = !labelFormat.isEmpty()
        ? labelFormat
        : (timeVisible
                  ? (secondsVisible ? QStringLiteral("MMM d HH:mm:ss") : QStringLiteral("MMM d HH:mm"))
                  : QStringLiteral("MMM d"));

    for (int index = first; index <= visibleTo; index += step) {
        if (index < 0 || index >= m_timePoints.size()) {
            continue;
        }
        const qreal localX = m_timeScale.indexToCoordinate(index);
        const qreal x = dataRect.left() + localX;
        if (x < dataRect.left() - normalizedMaxLabelWidth || x > dataRect.right() + normalizedMaxLabelWidth) {
            continue;
        }

        const QDateTime dateTime = QDateTime::fromSecsSinceEpoch(m_timePoints.at(index), QTimeZone::UTC);
        ticks.push_back({ index, m_timePoints.at(index), x, locale.toString(dateTime, format) });
    }
    return ticks;
}

void ChartModel::ensurePaneSizing(int paneCount)
{
    const int normalized = std::max(1, paneCount);
    const qsizetype oldHeightCount = m_paneHeights.size();
    const qsizetype oldStretchCount = m_paneStretchFactors.size();
    if (oldHeightCount < normalized) {
        m_paneHeights.resize(normalized);
    }
    if (oldStretchCount < normalized) {
        m_paneStretchFactors.resize(normalized);
        for (qsizetype i = oldStretchCount; i < m_paneStretchFactors.size(); ++i) {
            m_paneStretchFactors[i] = 1.0;
        }
    }
}

} // namespace QtFinCharts
