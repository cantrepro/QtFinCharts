#include "ChartTheme.h"

#include <algorithm>

namespace QtFinCharts {

ChartTheme::ChartTheme(QObject* parent)
    : QObject(parent)
{
}

void ChartTheme::setBackgroundType(const QString& type)
{
    setIfChanged(m_backgroundType, normalizeBackgroundType(type));
}

void ChartTheme::setBackgroundColor(const QColor& color)
{
    if (m_backgroundColor == color && m_backgroundTopColor == color && m_backgroundBottomColor == color) {
        return;
    }
    m_backgroundColor = color;
    m_backgroundTopColor = color;
    m_backgroundBottomColor = color;
    Q_EMIT changed();
}

void ChartTheme::setBackgroundTopColor(const QColor& color) { setIfChanged(m_backgroundTopColor, color); }
void ChartTheme::setBackgroundBottomColor(const QColor& color) { setIfChanged(m_backgroundBottomColor, color); }
void ChartTheme::setTextColor(const QColor& color) { setIfChanged(m_textColor, color); }
void ChartTheme::setGridColor(const QColor& color) { setIfChanged(m_gridColor, color); }

QColor ChartTheme::axisColor() const noexcept
{
    return m_priceAxisBorderColor == m_timeAxisBorderColor ? m_priceAxisBorderColor : QColor();
}

void ChartTheme::setAxisColor(const QColor& color)
{
    if (m_priceAxisBorderColor == color && m_timeAxisBorderColor == color) {
        return;
    }
    m_priceAxisBorderColor = color;
    m_timeAxisBorderColor = color;
    Q_EMIT changed();
}

void ChartTheme::setPriceAxisBorderColor(const QColor& color) { setIfChanged(m_priceAxisBorderColor, color); }
void ChartTheme::setTimeAxisBorderColor(const QColor& color) { setIfChanged(m_timeAxisBorderColor, color); }
void ChartTheme::setCrosshairColor(const QColor& color) { setIfChanged(m_crosshairColor, color); }
void ChartTheme::setCrosshairLabelBackground(const QColor& color) { setIfChanged(m_crosshairLabelBackground, color); }
void ChartTheme::setCrosshairLabelText(const QColor& color) { setIfChanged(m_crosshairLabelText, color); }
void ChartTheme::setPaneSeparatorColor(const QColor& color) { setIfChanged(m_paneSeparatorColor, color); }
void ChartTheme::setPaneSeparatorHoverColor(const QColor& color) { setIfChanged(m_paneSeparatorHoverColor, color); }
void ChartTheme::setFontFamily(const QString& family) { setIfChanged(m_fontFamily, family); }

void ChartTheme::setFontPixelSize(int size)
{
    setIfChanged(m_fontPixelSize, std::clamp(size, 8, 24));
}

QString ChartTheme::normalizeBackgroundType(const QString& type)
{
    if (type.compare(QStringLiteral("verticalGradient"), Qt::CaseInsensitive) == 0
        || type.compare(QStringLiteral("vertical-gradient"), Qt::CaseInsensitive) == 0) {
        return QStringLiteral("verticalGradient");
    }
    return QStringLiteral("solid");
}

template <typename T>
void ChartTheme::setIfChanged(T& member, const T& value)
{
    if (member == value) {
        return;
    }
    member = value;
    Q_EMIT changed();
}

} // namespace QtFinCharts
