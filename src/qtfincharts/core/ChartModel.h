#pragma once

#include "qtfincharts_export.h"

#include "PriceScale.h"
#include "SeriesModel.h"
#include "TimeScale.h"

#include <QtCore/QHash>
#include <QtCore/QLocale>
#include <QtCore/QSizeF>
#include <QtCore/QString>
#include <QtCore/QVector>

namespace QtFinCharts {

struct SeriesAttachment {
    const SeriesModel* model = nullptr;
    int pane = 0;
    bool visible = true;
    QString priceScaleId = QStringLiteral("right");
};

class QTFINCHARTS_EXPORT ChartModel {
public:
    void setSeries(QVector<SeriesAttachment> series);
    [[nodiscard]] const QVector<SeriesAttachment>& series() const noexcept { return m_series; }

    void rebuildTimeline();
    [[nodiscard]] const QVector<TimePoint>& timePoints() const noexcept { return m_timePoints; }
    [[nodiscard]] const QHash<TimePoint, int>& indexByTime() const noexcept { return m_indexByTime; }
    [[nodiscard]] int indexForTime(TimePoint time) const;
    [[nodiscard]] TimePoint timeAt(int index) const;

    TimeScale& timeScale() noexcept { return m_timeScale; }
    [[nodiscard]] const TimeScale& timeScale() const noexcept { return m_timeScale; }

    void setPaneCount(int paneCount);
    [[nodiscard]] int paneCount() const noexcept { return m_paneCount; }
    [[nodiscard]] int effectivePaneCount() const noexcept;
    void setPreserveEmptyPanes(bool preserve) noexcept { m_preserveEmptyPanes = preserve; }
    [[nodiscard]] bool preserveEmptyPanes() const noexcept { return m_preserveEmptyPanes; }
    void setPaneHeight(int pane, qreal height);
    void clearPaneHeight(int pane);
    [[nodiscard]] qreal paneHeight(int pane) const noexcept;
    void setPaneStretchFactor(int pane, qreal factor);
    [[nodiscard]] qreal paneStretchFactor(int pane) const noexcept;
    void removePaneSizing(int pane);
    void swapPaneSizing(int first, int second);

    [[nodiscard]] ChartLayout layoutForSize(const QSizeF& size,
        bool timeScaleVisible = true,
        bool priceScaleVisible = true,
        qreal timeAxisHeight = 30.0,
        qreal priceAxisMinimumWidth = 60.0,
        bool leftPriceScaleVisible = false,
        qreal leftPriceAxisMinimumWidth = 60.0) const;
    [[nodiscard]] PriceRange autoScaleRange(int pane,
        int from,
        int to,
        const QString& priceScaleId = QString()) const;
    [[nodiscard]] QVector<TimeTick> buildTimeTicks(const QRectF& dataRect,
        const QLocale& locale,
        bool timeVisible,
        bool secondsVisible,
        int maxLabelWidth = 76,
        const QString& labelFormat = QString(),
        bool uniformDistribution = true) const;

private:
    void ensurePaneSizing(int paneCount);

    QVector<SeriesAttachment> m_series;
    QVector<TimePoint> m_timePoints;
    QHash<TimePoint, int> m_indexByTime;
    TimeScale m_timeScale;
    int m_paneCount = 1;
    bool m_preserveEmptyPanes = true;
    QVector<qreal> m_paneHeights;
    QVector<qreal> m_paneStretchFactors;
};

} // namespace QtFinCharts
