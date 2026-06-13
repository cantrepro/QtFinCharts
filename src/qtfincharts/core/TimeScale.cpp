#include "TimeScale.h"

#include <algorithm>
#include <cmath>

namespace QtFinCharts {

void TimeScale::setWidth(qreal width)
{
    const qreal normalized = std::isfinite(width) ? std::max<qreal>(0.0, width) : 0.0;
    if (m_lockVisibleRangeOnResize && m_width > 0.0 && normalized > 0.0) {
        m_barSpacing *= normalized / m_width;
    }
    m_width = normalized;
    clampState();
}

void TimeScale::setPointCount(int count)
{
    setPointCount(count, false);
}

void TimeScale::setPointCount(int count, bool newBarsAddedToRight)
{
    const int oldPointCount = m_pointCount;
    const int oldBaseIndex = baseIndex();
    const qreal oldRightOffset = m_rightOffset;
    const LogicalRange oldVisibleRange = visibleLogicalRange();
    const bool oldLastBarVisible = oldPointCount > 0
        && oldVisibleRange.isValid()
        && oldVisibleRange.from <= oldBaseIndex
        && oldVisibleRange.to >= oldBaseIndex;

    m_pointCount = std::max(0, count);
    const int baseShift = baseIndex() - oldBaseIndex;
    if (newBarsAddedToRight
        && oldPointCount > 0
        && baseShift > 0
        && (!m_shiftVisibleRangeOnNewBar || !oldLastBarVisible)) {
        m_rightOffset = oldRightOffset - static_cast<qreal>(baseShift);
    }
    clampState();
}

void TimeScale::setBarSpacing(qreal spacing)
{
    if (!std::isfinite(spacing)) {
        return;
    }
    m_barSpacing = spacing;
    clampState();
}

void TimeScale::setRightOffset(qreal offset)
{
    if (!std::isfinite(offset)) {
        return;
    }
    m_rightOffset = offset;
    clampState();
}

void TimeScale::setSpacingLimits(qreal minSpacing, qreal maxSpacing)
{
    const qreal normalizedMin = std::isfinite(minSpacing)
        ? std::max<qreal>(0.1, minSpacing)
        : m_minBarSpacing;
    const qreal normalizedMax = std::isfinite(maxSpacing)
        ? maxSpacing
        : m_maxBarSpacing;
    m_minBarSpacing = normalizedMin;
    m_maxBarSpacing = std::max(m_minBarSpacing, normalizedMax);
    clampState();
}

void TimeScale::setFixedEdges(bool fixLeftEdge, bool fixRightEdge)
{
    m_fixLeftEdge = fixLeftEdge;
    m_fixRightEdge = fixRightEdge;
    clampState();
}

void TimeScale::setLockVisibleRangeOnResize(bool lock)
{
    m_lockVisibleRangeOnResize = lock;
}

void TimeScale::setRightBarStaysOnScroll(bool stays)
{
    m_rightBarStaysOnScroll = stays;
}

void TimeScale::setShiftVisibleRangeOnNewBar(bool shift)
{
    m_shiftVisibleRangeOnNewBar = shift;
}

qreal TimeScale::indexToCoordinate(double logicalIndex) const noexcept
{
    if (m_width <= 0.0 || m_pointCount <= 0 || m_barSpacing <= 0.0) {
        return 0.0;
    }

    const double deltaFromRight = static_cast<double>(baseIndex()) + m_rightOffset - logicalIndex;
    return m_width - (deltaFromRight + 0.5) * m_barSpacing - 1.0;
}

double TimeScale::coordinateToLogical(qreal x) const noexcept
{
    if (m_width <= 0.0 || m_pointCount <= 0 || m_barSpacing <= 0.0) {
        return 0.0;
    }

    return static_cast<double>(baseIndex()) + m_rightOffset + 0.5 - (m_width - 1.0 - x) / m_barSpacing;
}

int TimeScale::coordinateToIndex(qreal x) const noexcept
{
    return static_cast<int>(std::floor(coordinateToLogical(x) + 0.5));
}

LogicalRange TimeScale::visibleLogicalRange() const noexcept
{
    if (m_width <= 0.0 || m_pointCount <= 0) {
        return { };
    }

    return { coordinateToLogical(0.0), coordinateToLogical(std::max<qreal>(0.0, m_width - 1.0)) };
}

void TimeScale::setVisibleLogicalRange(const LogicalRange& range)
{
    if (!range.isValid()
        || !std::isfinite(range.from)
        || !std::isfinite(range.to)
        || m_width <= 1.0
        || m_pointCount <= 0
        || std::abs(range.to - range.from) <= 1e-9) {
        return;
    }

    m_barSpacing = (m_width - 1.0) / (range.to - range.from);
    m_rightOffset = range.to - static_cast<qreal>(baseIndex()) - 0.5;
    clampState();
}

std::pair<int, int> TimeScale::visibleIndexRange(int extraBars) const noexcept
{
    if (m_pointCount <= 0 || m_width <= 0.0) {
        return { 0, -1 };
    }

    constexpr double boundaryEpsilon = 1e-9;
    const LogicalRange range = visibleLogicalRange();
    const int from = std::max(0, static_cast<int>(std::floor(range.from + boundaryEpsilon)) - extraBars);
    const int to = std::min(m_pointCount - 1, static_cast<int>(std::ceil(range.to - boundaryEpsilon)) + extraBars);
    return { from, to };
}

void TimeScale::panByPixels(qreal deltaX)
{
    if (!std::isfinite(deltaX) || m_barSpacing <= 0.0) {
        return;
    }

    m_rightOffset -= deltaX / m_barSpacing;
    clampState();
}

void TimeScale::zoomAt(qreal x, qreal factor)
{
    if (!std::isfinite(x) || !std::isfinite(factor) || factor <= 0.0 || m_pointCount <= 0 || m_width <= 0.0) {
        return;
    }

    const double logicalBefore = coordinateToLogical(x);
    setBarSpacing(m_barSpacing * factor);
    if (!m_rightBarStaysOnScroll) {
        const double logicalAfter = coordinateToLogical(x);
        m_rightOffset += logicalBefore - logicalAfter;
        clampState();
    }
}

void TimeScale::fitContent(qreal leftMarginBars, qreal rightMarginBars)
{
    if (m_width <= 0.0 || m_pointCount <= 0) {
        return;
    }

    const qreal leftMargin = std::isfinite(leftMarginBars) ? std::max<qreal>(0.0, leftMarginBars) : 0.5;
    const qreal rightMargin = std::isfinite(rightMarginBars) ? std::max<qreal>(0.0, rightMarginBars) : 0.5;
    const qreal logicalWidth = std::max<qreal>(1.0, m_pointCount + leftMargin + rightMargin);
    m_barSpacing = m_width / logicalWidth;
    m_rightOffset = rightMargin;
    clampState();
}

void TimeScale::clampState()
{
    if (!std::isfinite(m_minBarSpacing) || m_minBarSpacing < 0.1) {
        m_minBarSpacing = 0.1;
    }
    if (!std::isfinite(m_maxBarSpacing) || m_maxBarSpacing < m_minBarSpacing) {
        m_maxBarSpacing = m_minBarSpacing;
    }

    const qreal effectiveMin = effectiveMinBarSpacing();
    const qreal effectiveMax = effectiveMaxBarSpacing(effectiveMin);
    m_barSpacing = std::isfinite(m_barSpacing)
        ? std::clamp(m_barSpacing, effectiveMin, effectiveMax)
        : effectiveMin;

    if (m_pointCount <= 0) {
        m_rightOffset = 0.0;
        return;
    }

    qreal minOffset = -static_cast<qreal>(m_pointCount);
    if (m_fixLeftEdge && m_width > 0.0 && m_barSpacing > 0.0) {
        minOffset = std::max(minOffset, m_width / m_barSpacing - static_cast<qreal>(m_pointCount));
    }

    const qreal maxOffset = std::max<qreal>(m_pointCount, m_width / std::max<qreal>(m_barSpacing, 1.0));
    const qreal effectiveMaxOffset = m_fixRightEdge ? 0.0 : maxOffset;
    m_rightOffset = std::isfinite(m_rightOffset)
        ? std::clamp(m_rightOffset, minOffset, std::max(minOffset, effectiveMaxOffset))
        : 0.0;
}

qreal TimeScale::effectiveMinBarSpacing() const noexcept
{
    qreal minimum = m_minBarSpacing;
    if (m_fixLeftEdge && m_fixRightEdge && m_pointCount > 0 && m_width > 0.0) {
        minimum = std::max(minimum, m_width / static_cast<qreal>(m_pointCount));
    }
    return minimum;
}

qreal TimeScale::effectiveMaxBarSpacing(qreal effectiveMin) const noexcept
{
    return std::max(effectiveMin, m_maxBarSpacing);
}

} // namespace QtFinCharts
