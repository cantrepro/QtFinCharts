#pragma once

#include "qtfincharts_export.h"

#include "Types.h"

#include <QtCore/QVector>

namespace QtFinCharts {

class QTFINCHARTS_EXPORT TimeScale {
public:
    void setWidth(qreal width);
    [[nodiscard]] qreal width() const noexcept { return m_width; }

    void setPointCount(int count);
    void setPointCount(int count, bool newBarsAddedToRight);
    [[nodiscard]] int pointCount() const noexcept { return m_pointCount; }
    [[nodiscard]] int baseIndex() const noexcept { return std::max(0, m_pointCount - 1); }

    void setBarSpacing(qreal spacing);
    [[nodiscard]] qreal barSpacing() const noexcept { return m_barSpacing; }

    void setRightOffset(qreal offset);
    [[nodiscard]] qreal rightOffset() const noexcept { return m_rightOffset; }

    void setSpacingLimits(qreal minSpacing, qreal maxSpacing);
    [[nodiscard]] qreal minBarSpacing() const noexcept { return m_minBarSpacing; }
    [[nodiscard]] qreal maxBarSpacing() const noexcept { return m_maxBarSpacing; }

    void setFixedEdges(bool fixLeftEdge, bool fixRightEdge);
    [[nodiscard]] bool fixLeftEdge() const noexcept { return m_fixLeftEdge; }
    [[nodiscard]] bool fixRightEdge() const noexcept { return m_fixRightEdge; }

    void setLockVisibleRangeOnResize(bool lock);
    [[nodiscard]] bool lockVisibleRangeOnResize() const noexcept { return m_lockVisibleRangeOnResize; }

    void setRightBarStaysOnScroll(bool stays);
    [[nodiscard]] bool rightBarStaysOnScroll() const noexcept { return m_rightBarStaysOnScroll; }

    void setShiftVisibleRangeOnNewBar(bool shift);
    [[nodiscard]] bool shiftVisibleRangeOnNewBar() const noexcept { return m_shiftVisibleRangeOnNewBar; }

    [[nodiscard]] qreal indexToCoordinate(double logicalIndex) const noexcept;
    [[nodiscard]] double coordinateToLogical(qreal x) const noexcept;
    [[nodiscard]] int coordinateToIndex(qreal x) const noexcept;
    [[nodiscard]] LogicalRange visibleLogicalRange() const noexcept;
    void setVisibleLogicalRange(const LogicalRange& range);
    [[nodiscard]] std::pair<int, int> visibleIndexRange(int extraBars = 1) const noexcept;

    void panByPixels(qreal deltaX);
    void zoomAt(qreal x, qreal factor);
    void fitContent(qreal leftMarginBars = 0.5, qreal rightMarginBars = 0.5);

private:
    void clampState();
    [[nodiscard]] qreal effectiveMinBarSpacing() const noexcept;
    [[nodiscard]] qreal effectiveMaxBarSpacing(qreal effectiveMin) const noexcept;

    qreal m_width = 0.0;
    int m_pointCount = 0;
    qreal m_barSpacing = 8.0;
    qreal m_rightOffset = 2.0;
    qreal m_minBarSpacing = 2.0;
    qreal m_maxBarSpacing = 80.0;
    bool m_fixLeftEdge = false;
    bool m_fixRightEdge = false;
    bool m_lockVisibleRangeOnResize = false;
    bool m_rightBarStaysOnScroll = false;
    bool m_shiftVisibleRangeOnNewBar = true;
};

} // namespace QtFinCharts
