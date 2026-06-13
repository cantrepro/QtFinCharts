#pragma once

#include "qtfincharts_export.h"

#include "ChartSeries.h"

#include <QtGui/QColor>
#include <QtQml/qqmlregistration.h>

namespace QtFinCharts {

class QTFINCHARTS_EXPORT AreaSeries : public ChartSeries {
    Q_OBJECT
    QML_ELEMENT

    Q_PROPERTY(QColor topColor READ topColor WRITE setTopColor NOTIFY optionsChanged)
    Q_PROPERTY(QColor bottomColor READ bottomColor WRITE setBottomColor NOTIFY optionsChanged)
    Q_PROPERTY(bool relativeGradient READ relativeGradient WRITE setRelativeGradient NOTIFY optionsChanged)
    Q_PROPERTY(bool invertFilledArea READ invertFilledArea WRITE setInvertFilledArea NOTIFY optionsChanged)
    Q_PROPERTY(QColor lineColor READ lineColor WRITE setLineColor NOTIFY optionsChanged)
    Q_PROPERTY(int lineStyle READ lineStyle WRITE setLineStyle NOTIFY optionsChanged)
    Q_PROPERTY(int lineType READ lineType WRITE setLineType NOTIFY optionsChanged)
    Q_PROPERTY(qreal lineWidth READ lineWidth WRITE setLineWidth NOTIFY optionsChanged)
    Q_PROPERTY(bool lineVisible READ lineVisible WRITE setLineVisible NOTIFY optionsChanged)
    Q_PROPERTY(bool pointMarkersVisible READ pointMarkersVisible WRITE setPointMarkersVisible NOTIFY optionsChanged)
    Q_PROPERTY(qreal pointMarkersRadius READ pointMarkersRadius WRITE setPointMarkersRadius NOTIFY optionsChanged)
    Q_PROPERTY(bool crosshairMarkerVisible READ crosshairMarkerVisible WRITE setCrosshairMarkerVisible NOTIFY optionsChanged)
    Q_PROPERTY(qreal crosshairMarkerRadius READ crosshairMarkerRadius WRITE setCrosshairMarkerRadius NOTIFY optionsChanged)
    Q_PROPERTY(QColor crosshairMarkerBorderColor READ crosshairMarkerBorderColor WRITE setCrosshairMarkerBorderColor NOTIFY optionsChanged)
    Q_PROPERTY(qreal crosshairMarkerBorderWidth READ crosshairMarkerBorderWidth WRITE setCrosshairMarkerBorderWidth NOTIFY optionsChanged)
    Q_PROPERTY(QColor crosshairMarkerBackgroundColor READ crosshairMarkerBackgroundColor WRITE setCrosshairMarkerBackgroundColor NOTIFY optionsChanged)

public:
    explicit AreaSeries(QObject* parent = nullptr);

    [[nodiscard]] QColor topColor() const noexcept { return m_topColor; }
    void setTopColor(const QColor& color);

    [[nodiscard]] QColor bottomColor() const noexcept { return m_bottomColor; }
    void setBottomColor(const QColor& color);

    [[nodiscard]] bool relativeGradient() const noexcept { return m_relativeGradient; }
    void setRelativeGradient(bool relative);

    [[nodiscard]] bool invertFilledArea() const noexcept { return m_invertFilledArea; }
    void setInvertFilledArea(bool invert);

    [[nodiscard]] QColor lineColor() const noexcept { return m_lineColor; }
    void setLineColor(const QColor& color);

    [[nodiscard]] int lineStyle() const noexcept { return m_lineStyle; }
    void setLineStyle(int style);

    [[nodiscard]] int lineType() const noexcept { return m_lineType; }
    void setLineType(int type);

    [[nodiscard]] qreal lineWidth() const noexcept { return m_lineWidth; }
    void setLineWidth(qreal width);

    [[nodiscard]] bool lineVisible() const noexcept { return m_lineVisible; }
    void setLineVisible(bool visible);

    [[nodiscard]] bool pointMarkersVisible() const noexcept { return m_pointMarkersVisible; }
    void setPointMarkersVisible(bool visible);

    [[nodiscard]] qreal pointMarkersRadius() const noexcept { return m_pointMarkersRadius; }
    void setPointMarkersRadius(qreal radius);

    [[nodiscard]] bool crosshairMarkerVisible() const noexcept { return m_crosshairMarkerVisible; }
    void setCrosshairMarkerVisible(bool visible);

    [[nodiscard]] qreal crosshairMarkerRadius() const noexcept { return m_crosshairMarkerRadius; }
    void setCrosshairMarkerRadius(qreal radius);

    [[nodiscard]] QColor crosshairMarkerBorderColor() const noexcept { return m_crosshairMarkerBorderColor; }
    void setCrosshairMarkerBorderColor(const QColor& color);

    [[nodiscard]] qreal crosshairMarkerBorderWidth() const noexcept { return m_crosshairMarkerBorderWidth; }
    void setCrosshairMarkerBorderWidth(qreal width);

    [[nodiscard]] QColor crosshairMarkerBackgroundColor() const noexcept { return m_crosshairMarkerBackgroundColor; }
    void setCrosshairMarkerBackgroundColor(const QColor& color);

    void setData(const QVariantList& rows) override;
    void update(const QVariantMap& row, bool historicalUpdate = false) override;

private:
    [[nodiscard]] static bool parseRow(const QVariantMap& row, LineDataPoint* point);

    QColor m_topColor = QColor(41, 98, 255, 90);
    QColor m_bottomColor = QColor(41, 98, 255, 18);
    bool m_relativeGradient = false;
    bool m_invertFilledArea = false;
    QColor m_lineColor = QColor("#2962ff");
    int m_lineStyle = static_cast<int>(LineStyle::Solid);
    int m_lineType = static_cast<int>(LineType::Simple);
    qreal m_lineWidth = 2.0;
    bool m_lineVisible = true;
    bool m_pointMarkersVisible = false;
    qreal m_pointMarkersRadius = 0.0;
    bool m_crosshairMarkerVisible = true;
    qreal m_crosshairMarkerRadius = 4.0;
    QColor m_crosshairMarkerBorderColor;
    qreal m_crosshairMarkerBorderWidth = 2.0;
    QColor m_crosshairMarkerBackgroundColor;
};

} // namespace QtFinCharts
