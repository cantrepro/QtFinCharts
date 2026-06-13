#pragma once

#include "qtfincharts_export.h"

#include "../core/ChartModel.h"
#include "ChartTheme.h"

#include <QtCore/QHash>
#include <QtCore/QLocale>
#include <QtCore/QPointer>
#include <QtCore/QSize>
#include <QtCore/QVariantList>
#include <QtCore/QVariantMap>
#include <QtGui/QColor>
#include <QtQml/QJSValue>
#include <QtQml/QQmlListProperty>
#include <QtQml/qqmlregistration.h>
#include <QtQuick/QQuickItem>

#include <limits>
#include <memory>
#include <optional>

namespace QtFinCharts {

class ChartSeries;

class QTFINCHARTS_EXPORT FinancialChartItem : public QQuickItem {
    Q_OBJECT
    QML_NAMED_ELEMENT(FinancialChart)
    Q_CLASSINFO("DefaultProperty", "series")

    Q_PROPERTY(QQmlListProperty<QtFinCharts::ChartSeries> series READ series)
    Q_PROPERTY(QtFinCharts::ChartTheme* theme READ theme WRITE setTheme NOTIFY themeChanged)
    Q_PROPERTY(int paneCount READ paneCount WRITE setPaneCount NOTIFY paneCountChanged)
    Q_PROPERTY(bool gridVisible READ gridVisible WRITE setGridVisible NOTIFY gridVisibleChanged)
    Q_PROPERTY(bool verticalGridVisible READ verticalGridVisible WRITE setVerticalGridVisible NOTIFY gridVisibleChanged)
    Q_PROPERTY(bool horizontalGridVisible READ horizontalGridVisible WRITE setHorizontalGridVisible NOTIFY gridVisibleChanged)
    Q_PROPERTY(int gridLineStyle READ gridLineStyle WRITE setGridLineStyle NOTIFY gridVisibleChanged)
    Q_PROPERTY(int verticalGridLineStyle READ verticalGridLineStyle WRITE setVerticalGridLineStyle NOTIFY gridVisibleChanged)
    Q_PROPERTY(int horizontalGridLineStyle READ horizontalGridLineStyle WRITE setHorizontalGridLineStyle NOTIFY gridVisibleChanged)
    Q_PROPERTY(bool interactive READ isInteractive WRITE setInteractive NOTIFY interactiveChanged)
    Q_PROPERTY(bool handleScroll READ handleScroll WRITE setHandleScroll NOTIFY interactionOptionsChanged)
    Q_PROPERTY(bool handleScale READ handleScale WRITE setHandleScale NOTIFY interactionOptionsChanged)
    Q_PROPERTY(bool mouseWheelScroll READ mouseWheelScroll WRITE setMouseWheelScroll NOTIFY interactionOptionsChanged)
    Q_PROPERTY(bool mouseWheelScale READ mouseWheelScale WRITE setMouseWheelScale NOTIFY interactionOptionsChanged)
    Q_PROPERTY(bool pressedMouseMoveScroll READ pressedMouseMoveScroll WRITE setPressedMouseMoveScroll NOTIFY interactionOptionsChanged)
    Q_PROPERTY(bool horizontalTouchDrag READ horizontalTouchDrag WRITE setHorizontalTouchDrag NOTIFY interactionOptionsChanged)
    Q_PROPERTY(bool verticalTouchDrag READ verticalTouchDrag WRITE setVerticalTouchDrag NOTIFY interactionOptionsChanged)
    Q_PROPERTY(bool pinchScale READ pinchScale WRITE setPinchScale NOTIFY interactionOptionsChanged)
    Q_PROPERTY(bool timeScaleVisible READ timeScaleVisible WRITE setTimeScaleVisible NOTIFY timeScaleVisibleChanged)
    Q_PROPERTY(bool priceScaleVisible READ priceScaleVisible WRITE setPriceScaleVisible NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool leftPriceScaleVisible READ leftPriceScaleVisible WRITE setLeftPriceScaleVisible NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool priceAxisBorderVisible READ priceAxisBorderVisible WRITE setPriceAxisBorderVisible NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool leftPriceAxisBorderVisible READ leftPriceAxisBorderVisible WRITE setLeftPriceAxisBorderVisible NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool timeAxisBorderVisible READ timeAxisBorderVisible WRITE setTimeAxisBorderVisible NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool priceAxisTickMarksVisible READ priceAxisTickMarksVisible WRITE setPriceAxisTickMarksVisible NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool leftPriceAxisTickMarksVisible READ leftPriceAxisTickMarksVisible WRITE setLeftPriceAxisTickMarksVisible NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool timeAxisTickMarksVisible READ timeAxisTickMarksVisible WRITE setTimeAxisTickMarksVisible NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool timeVisible READ timeVisible WRITE setTimeVisible NOTIFY localizationChanged)
    Q_PROPERTY(bool secondsVisible READ secondsVisible WRITE setSecondsVisible NOTIFY localizationChanged)
    Q_PROPERTY(QString dateFormat READ dateFormat WRITE setDateFormat NOTIFY localizationChanged)
    Q_PROPERTY(QString timeFormat READ timeFormat WRITE setTimeFormat NOTIFY localizationChanged)
    Q_PROPERTY(QString timeWithSecondsFormat READ timeWithSecondsFormat WRITE setTimeWithSecondsFormat NOTIFY localizationChanged)
    Q_PROPERTY(qreal timeAxisMinimumHeight READ timeAxisMinimumHeight WRITE setTimeAxisMinimumHeight NOTIFY axisOptionsChanged)
    Q_PROPERTY(int timeAxisMaxLabelWidth READ timeAxisMaxLabelWidth WRITE setTimeAxisMaxLabelWidth NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool timeAxisUniformDistribution READ timeAxisUniformDistribution WRITE setTimeAxisUniformDistribution NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool timeAxisBoldLabels READ timeAxisBoldLabels WRITE setTimeAxisBoldLabels NOTIFY axisOptionsChanged)
    Q_PROPERTY(qreal priceAxisMinimumWidth READ priceAxisMinimumWidth WRITE setPriceAxisMinimumWidth NOTIFY axisOptionsChanged)
    Q_PROPERTY(qreal leftPriceAxisMinimumWidth READ leftPriceAxisMinimumWidth WRITE setLeftPriceAxisMinimumWidth NOTIFY axisOptionsChanged)
    Q_PROPERTY(QColor priceAxisTextColor READ priceAxisTextColor WRITE setPriceAxisTextColor NOTIFY axisOptionsChanged)
    Q_PROPERTY(QString priceAxisLabelAlignment READ priceAxisLabelAlignment WRITE setPriceAxisLabelAlignment NOTIFY axisOptionsChanged)
    Q_PROPERTY(qreal priceScaleTopMargin READ priceScaleTopMargin WRITE setPriceScaleTopMargin NOTIFY axisOptionsChanged)
    Q_PROPERTY(qreal priceScaleBottomMargin READ priceScaleBottomMargin WRITE setPriceScaleBottomMargin NOTIFY axisOptionsChanged)
    Q_PROPERTY(qreal priceTickSpacing READ priceTickSpacing WRITE setPriceTickSpacing NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool autoScale READ autoScale WRITE setAutoScale NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool priceScaleInverted READ priceScaleInverted WRITE setPriceScaleInverted NOTIFY axisOptionsChanged)
    Q_PROPERTY(QString priceScaleMode READ priceScaleMode WRITE setPriceScaleMode NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool preserveEmptyPanes READ preserveEmptyPanes WRITE setPreserveEmptyPanes NOTIFY paneCountChanged)
    Q_PROPERTY(bool paneSeparatorsResizable READ paneSeparatorsResizable WRITE setPaneSeparatorsResizable NOTIFY paneSeparatorOptionsChanged)
    Q_PROPERTY(QString localeName READ localeName WRITE setLocaleName NOTIFY localizationChanged)
    Q_PROPERTY(int pricePrecision READ pricePrecision WRITE setPricePrecision NOTIFY localizationChanged)
    Q_PROPERTY(QString priceFormat READ priceFormat WRITE setPriceFormat NOTIFY localizationChanged)
    Q_PROPERTY(QJSValue timeFormatter READ timeFormatter WRITE setTimeFormatter NOTIFY localizationChanged)
    Q_PROPERTY(QJSValue timeTickMarkFormatter READ timeTickMarkFormatter WRITE setTimeTickMarkFormatter NOTIFY localizationChanged)
    Q_PROPERTY(QJSValue priceFormatter READ priceFormatter WRITE setPriceFormatter NOTIFY localizationChanged)
    Q_PROPERTY(QJSValue percentageFormatter READ percentageFormatter WRITE setPercentageFormatter NOTIFY localizationChanged)
    Q_PROPERTY(QJSValue priceTickMarkFormatter READ priceTickMarkFormatter WRITE setPriceTickMarkFormatter NOTIFY localizationChanged)
    Q_PROPERTY(QJSValue percentageTickMarkFormatter READ percentageTickMarkFormatter WRITE setPercentageTickMarkFormatter NOTIFY localizationChanged)
    Q_PROPERTY(qreal barSpacing READ barSpacing WRITE setBarSpacing NOTIFY barSpacingChanged)
    Q_PROPERTY(qreal rightOffset READ rightOffset WRITE setRightOffset NOTIFY rightOffsetChanged)
    Q_PROPERTY(qreal rightOffsetPixels READ rightOffsetPixels WRITE setRightOffsetPixels NOTIFY rightOffsetChanged)
    Q_PROPERTY(qreal minBarSpacing READ minBarSpacing WRITE setMinBarSpacing NOTIFY barSpacingChanged)
    Q_PROPERTY(qreal maxBarSpacing READ maxBarSpacing WRITE setMaxBarSpacing NOTIFY barSpacingChanged)
    Q_PROPERTY(bool fixLeftEdge READ fixLeftEdge WRITE setFixLeftEdge NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool fixRightEdge READ fixRightEdge WRITE setFixRightEdge NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool lockVisibleTimeRangeOnResize READ lockVisibleTimeRangeOnResize WRITE setLockVisibleTimeRangeOnResize NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool rightBarStaysOnScroll READ rightBarStaysOnScroll WRITE setRightBarStaysOnScroll NOTIFY axisOptionsChanged)
    Q_PROPERTY(bool shiftVisibleRangeOnNewBar READ shiftVisibleRangeOnNewBar WRITE setShiftVisibleRangeOnNewBar NOTIFY axisOptionsChanged)
    Q_PROPERTY(int crosshairMode READ crosshairMode WRITE setCrosshairMode NOTIFY crosshairOptionsChanged)
    Q_PROPERTY(bool verticalCrosshairLineVisible READ verticalCrosshairLineVisible WRITE setVerticalCrosshairLineVisible NOTIFY crosshairOptionsChanged)
    Q_PROPERTY(bool horizontalCrosshairLineVisible READ horizontalCrosshairLineVisible WRITE setHorizontalCrosshairLineVisible NOTIFY crosshairOptionsChanged)
    Q_PROPERTY(QColor verticalCrosshairLineColor READ verticalCrosshairLineColor WRITE setVerticalCrosshairLineColor NOTIFY crosshairOptionsChanged)
    Q_PROPERTY(QColor horizontalCrosshairLineColor READ horizontalCrosshairLineColor WRITE setHorizontalCrosshairLineColor NOTIFY crosshairOptionsChanged)
    Q_PROPERTY(qreal verticalCrosshairLineWidth READ verticalCrosshairLineWidth WRITE setVerticalCrosshairLineWidth NOTIFY crosshairOptionsChanged)
    Q_PROPERTY(qreal horizontalCrosshairLineWidth READ horizontalCrosshairLineWidth WRITE setHorizontalCrosshairLineWidth NOTIFY crosshairOptionsChanged)
    Q_PROPERTY(int verticalCrosshairLineStyle READ verticalCrosshairLineStyle WRITE setVerticalCrosshairLineStyle NOTIFY crosshairOptionsChanged)
    Q_PROPERTY(int horizontalCrosshairLineStyle READ horizontalCrosshairLineStyle WRITE setHorizontalCrosshairLineStyle NOTIFY crosshairOptionsChanged)
    Q_PROPERTY(bool timeCrosshairLabelVisible READ timeCrosshairLabelVisible WRITE setTimeCrosshairLabelVisible NOTIFY crosshairOptionsChanged)
    Q_PROPERTY(bool priceCrosshairLabelVisible READ priceCrosshairLabelVisible WRITE setPriceCrosshairLabelVisible NOTIFY crosshairOptionsChanged)
    Q_PROPERTY(QColor timeCrosshairLabelBackground READ timeCrosshairLabelBackground WRITE setTimeCrosshairLabelBackground NOTIFY crosshairOptionsChanged)
    Q_PROPERTY(QColor priceCrosshairLabelBackground READ priceCrosshairLabelBackground WRITE setPriceCrosshairLabelBackground NOTIFY crosshairOptionsChanged)
    Q_PROPERTY(bool snapCrosshairToHiddenSeries READ snapCrosshairToHiddenSeries WRITE setSnapCrosshairToHiddenSeries NOTIFY crosshairOptionsChanged)
    Q_PROPERTY(bool removed READ isRemoved NOTIFY removedChanged)
    Q_PROPERTY(QQuickItem* nativeItem READ nativeItem CONSTANT)

public:
    explicit FinancialChartItem(QQuickItem* parent = nullptr);
    ~FinancialChartItem() override;

    [[nodiscard]] QQmlListProperty<ChartSeries> series();

    [[nodiscard]] ChartTheme* theme() const noexcept { return m_theme ? m_theme.data() : m_ownedTheme.get(); }
    void setTheme(ChartTheme* theme);

    [[nodiscard]] int paneCount() const noexcept { return m_paneCount; }
    void setPaneCount(int count);

    [[nodiscard]] bool gridVisible() const noexcept { return m_gridVisible; }
    void setGridVisible(bool visible);

    [[nodiscard]] bool verticalGridVisible() const noexcept { return m_verticalGridVisible; }
    void setVerticalGridVisible(bool visible);

    [[nodiscard]] bool horizontalGridVisible() const noexcept { return m_horizontalGridVisible; }
    void setHorizontalGridVisible(bool visible);

    [[nodiscard]] int gridLineStyle() const noexcept;
    void setGridLineStyle(int style);

    [[nodiscard]] int verticalGridLineStyle() const noexcept { return m_verticalGridLineStyle; }
    void setVerticalGridLineStyle(int style);

    [[nodiscard]] int horizontalGridLineStyle() const noexcept { return m_horizontalGridLineStyle; }
    void setHorizontalGridLineStyle(int style);

    [[nodiscard]] bool isInteractive() const noexcept { return m_interactive; }
    void setInteractive(bool interactive);

    [[nodiscard]] bool handleScroll() const noexcept { return m_handleScroll; }
    void setHandleScroll(bool enabled);

    [[nodiscard]] bool handleScale() const noexcept { return m_handleScale; }
    void setHandleScale(bool enabled);

    [[nodiscard]] bool mouseWheelScroll() const noexcept { return m_mouseWheelScroll; }
    void setMouseWheelScroll(bool enabled);

    [[nodiscard]] bool mouseWheelScale() const noexcept { return m_mouseWheelScale; }
    void setMouseWheelScale(bool enabled);

    [[nodiscard]] bool pressedMouseMoveScroll() const noexcept { return m_pressedMouseMoveScroll; }
    void setPressedMouseMoveScroll(bool enabled);

    [[nodiscard]] bool horizontalTouchDrag() const noexcept { return m_horizontalTouchDrag; }
    void setHorizontalTouchDrag(bool enabled);

    [[nodiscard]] bool verticalTouchDrag() const noexcept { return m_verticalTouchDrag; }
    void setVerticalTouchDrag(bool enabled);

    [[nodiscard]] bool pinchScale() const noexcept { return m_pinchScale; }
    void setPinchScale(bool enabled);

    [[nodiscard]] bool timeScaleVisible() const noexcept { return m_timeScaleVisible; }
    void setTimeScaleVisible(bool visible);

    [[nodiscard]] bool priceScaleVisible() const noexcept { return m_priceScaleVisible; }
    void setPriceScaleVisible(bool visible);

    [[nodiscard]] bool leftPriceScaleVisible() const noexcept { return m_leftPriceScaleVisible; }
    void setLeftPriceScaleVisible(bool visible);

    [[nodiscard]] bool priceAxisBorderVisible() const noexcept { return m_priceAxisBorderVisible; }
    void setPriceAxisBorderVisible(bool visible);

    [[nodiscard]] bool leftPriceAxisBorderVisible() const noexcept { return m_leftPriceAxisBorderVisible; }
    void setLeftPriceAxisBorderVisible(bool visible);

    [[nodiscard]] bool timeAxisBorderVisible() const noexcept { return m_timeAxisBorderVisible; }
    void setTimeAxisBorderVisible(bool visible);

    [[nodiscard]] bool priceAxisTickMarksVisible() const noexcept { return m_priceAxisTickMarksVisible; }
    void setPriceAxisTickMarksVisible(bool visible);

    [[nodiscard]] bool leftPriceAxisTickMarksVisible() const noexcept { return m_leftPriceAxisTickMarksVisible; }
    void setLeftPriceAxisTickMarksVisible(bool visible);

    [[nodiscard]] bool timeAxisTickMarksVisible() const noexcept { return m_timeAxisTickMarksVisible; }
    void setTimeAxisTickMarksVisible(bool visible);

    [[nodiscard]] bool timeVisible() const noexcept { return m_timeVisible; }
    void setTimeVisible(bool visible);

    [[nodiscard]] bool secondsVisible() const noexcept { return m_secondsVisible; }
    void setSecondsVisible(bool visible);

    [[nodiscard]] QString dateFormat() const { return m_dateFormat; }
    void setDateFormat(const QString& format);

    [[nodiscard]] QString timeFormat() const { return m_timeFormat; }
    void setTimeFormat(const QString& format);

    [[nodiscard]] QString timeWithSecondsFormat() const { return m_timeWithSecondsFormat; }
    void setTimeWithSecondsFormat(const QString& format);

    [[nodiscard]] qreal timeAxisMinimumHeight() const noexcept { return m_timeAxisMinimumHeight; }
    void setTimeAxisMinimumHeight(qreal height);

    [[nodiscard]] int timeAxisMaxLabelWidth() const noexcept { return m_timeAxisMaxLabelWidth; }
    void setTimeAxisMaxLabelWidth(int width);

    [[nodiscard]] bool timeAxisUniformDistribution() const noexcept { return m_timeAxisUniformDistribution; }
    void setTimeAxisUniformDistribution(bool uniform);

    [[nodiscard]] bool timeAxisBoldLabels() const noexcept { return m_timeAxisBoldLabels; }
    void setTimeAxisBoldLabels(bool bold);

    [[nodiscard]] qreal priceAxisMinimumWidth() const noexcept { return m_priceAxisMinimumWidth; }
    void setPriceAxisMinimumWidth(qreal width);

    [[nodiscard]] qreal leftPriceAxisMinimumWidth() const noexcept { return m_leftPriceAxisMinimumWidth; }
    void setLeftPriceAxisMinimumWidth(qreal width);

    [[nodiscard]] QColor priceAxisTextColor() const noexcept { return m_priceAxisTextColor; }
    void setPriceAxisTextColor(const QColor& color);

    [[nodiscard]] QString priceAxisLabelAlignment() const { return m_priceAxisLabelAlignment; }
    void setPriceAxisLabelAlignment(const QString& alignment);

    [[nodiscard]] qreal priceScaleTopMargin() const noexcept { return m_priceScaleTopMargin; }
    void setPriceScaleTopMargin(qreal margin);

    [[nodiscard]] qreal priceScaleBottomMargin() const noexcept { return m_priceScaleBottomMargin; }
    void setPriceScaleBottomMargin(qreal margin);

    [[nodiscard]] qreal priceTickSpacing() const noexcept { return m_priceTickSpacing; }
    void setPriceTickSpacing(qreal spacing);

    [[nodiscard]] bool autoScale() const noexcept { return m_autoScale; }
    void setAutoScale(bool enabled);

    [[nodiscard]] bool priceScaleInverted() const noexcept { return m_priceScaleInverted; }
    void setPriceScaleInverted(bool inverted);

    [[nodiscard]] QString priceScaleMode() const;
    void setPriceScaleMode(const QString& mode);

    [[nodiscard]] bool preserveEmptyPanes() const noexcept { return m_preserveEmptyPanes; }
    void setPreserveEmptyPanes(bool preserve);

    [[nodiscard]] bool paneSeparatorsResizable() const noexcept { return m_paneSeparatorsResizable; }
    void setPaneSeparatorsResizable(bool resizable);

    [[nodiscard]] QString localeName() const { return m_locale.name(); }
    void setLocaleName(const QString& localeName);

    [[nodiscard]] int pricePrecision() const noexcept { return m_pricePrecision; }
    void setPricePrecision(int precision);

    [[nodiscard]] QString priceFormat() const;
    void setPriceFormat(const QString& format);

    [[nodiscard]] QJSValue timeFormatter() const { return m_timeFormatter; }
    void setTimeFormatter(const QJSValue& formatter);

    [[nodiscard]] QJSValue timeTickMarkFormatter() const { return m_timeTickMarkFormatter; }
    void setTimeTickMarkFormatter(const QJSValue& formatter);

    [[nodiscard]] QJSValue priceFormatter() const { return m_priceFormatter; }
    void setPriceFormatter(const QJSValue& formatter);

    [[nodiscard]] QJSValue percentageFormatter() const { return m_percentageFormatter; }
    void setPercentageFormatter(const QJSValue& formatter);

    [[nodiscard]] QJSValue priceTickMarkFormatter() const { return m_priceTickMarkFormatter; }
    void setPriceTickMarkFormatter(const QJSValue& formatter);

    [[nodiscard]] QJSValue percentageTickMarkFormatter() const { return m_percentageTickMarkFormatter; }
    void setPercentageTickMarkFormatter(const QJSValue& formatter);

    [[nodiscard]] qreal barSpacing() const noexcept { return m_model.timeScale().barSpacing(); }
    void setBarSpacing(qreal spacing);

    [[nodiscard]] qreal rightOffset() const noexcept { return m_model.timeScale().rightOffset(); }
    void setRightOffset(qreal offset);

    [[nodiscard]] qreal rightOffsetPixels() const noexcept { return m_model.timeScale().rightOffset() * m_model.timeScale().barSpacing(); }
    void setRightOffsetPixels(qreal offsetPixels);

    [[nodiscard]] qreal minBarSpacing() const noexcept { return m_model.timeScale().minBarSpacing(); }
    void setMinBarSpacing(qreal spacing);

    [[nodiscard]] qreal maxBarSpacing() const noexcept { return m_model.timeScale().maxBarSpacing(); }
    void setMaxBarSpacing(qreal spacing);

    [[nodiscard]] bool fixLeftEdge() const noexcept { return m_model.timeScale().fixLeftEdge(); }
    void setFixLeftEdge(bool enabled);

    [[nodiscard]] bool fixRightEdge() const noexcept { return m_model.timeScale().fixRightEdge(); }
    void setFixRightEdge(bool enabled);

    [[nodiscard]] bool lockVisibleTimeRangeOnResize() const noexcept { return m_model.timeScale().lockVisibleRangeOnResize(); }
    void setLockVisibleTimeRangeOnResize(bool enabled);

    [[nodiscard]] bool rightBarStaysOnScroll() const noexcept { return m_model.timeScale().rightBarStaysOnScroll(); }
    void setRightBarStaysOnScroll(bool enabled);

    [[nodiscard]] bool shiftVisibleRangeOnNewBar() const noexcept { return m_model.timeScale().shiftVisibleRangeOnNewBar(); }
    void setShiftVisibleRangeOnNewBar(bool enabled);

    [[nodiscard]] int crosshairMode() const noexcept { return m_crosshairMode; }
    void setCrosshairMode(int mode);

    [[nodiscard]] bool verticalCrosshairLineVisible() const noexcept { return m_verticalCrosshairLineVisible; }
    void setVerticalCrosshairLineVisible(bool visible);

    [[nodiscard]] bool horizontalCrosshairLineVisible() const noexcept { return m_horizontalCrosshairLineVisible; }
    void setHorizontalCrosshairLineVisible(bool visible);

    [[nodiscard]] QColor verticalCrosshairLineColor() const noexcept { return m_verticalCrosshairLineColor; }
    void setVerticalCrosshairLineColor(const QColor& color);

    [[nodiscard]] QColor horizontalCrosshairLineColor() const noexcept { return m_horizontalCrosshairLineColor; }
    void setHorizontalCrosshairLineColor(const QColor& color);

    [[nodiscard]] qreal verticalCrosshairLineWidth() const noexcept { return m_verticalCrosshairLineWidth; }
    void setVerticalCrosshairLineWidth(qreal width);

    [[nodiscard]] qreal horizontalCrosshairLineWidth() const noexcept { return m_horizontalCrosshairLineWidth; }
    void setHorizontalCrosshairLineWidth(qreal width);

    [[nodiscard]] int verticalCrosshairLineStyle() const noexcept { return m_verticalCrosshairLineStyle; }
    void setVerticalCrosshairLineStyle(int style);

    [[nodiscard]] int horizontalCrosshairLineStyle() const noexcept { return m_horizontalCrosshairLineStyle; }
    void setHorizontalCrosshairLineStyle(int style);

    [[nodiscard]] bool timeCrosshairLabelVisible() const noexcept { return m_timeCrosshairLabelVisible; }
    void setTimeCrosshairLabelVisible(bool visible);

    [[nodiscard]] bool priceCrosshairLabelVisible() const noexcept { return m_priceCrosshairLabelVisible; }
    void setPriceCrosshairLabelVisible(bool visible);

    [[nodiscard]] QColor timeCrosshairLabelBackground() const noexcept { return m_timeCrosshairLabelBackground; }
    void setTimeCrosshairLabelBackground(const QColor& color);

    [[nodiscard]] QColor priceCrosshairLabelBackground() const noexcept { return m_priceCrosshairLabelBackground; }
    void setPriceCrosshairLabelBackground(const QColor& color);

    [[nodiscard]] bool snapCrosshairToHiddenSeries() const noexcept { return m_snapCrosshairToHiddenSeries; }
    void setSnapCrosshairToHiddenSeries(bool snap);

    Q_INVOKABLE void fitContent();
    Q_INVOKABLE void resetTimeScale();
    Q_INVOKABLE int addPane();
    Q_INVOKABLE void removePane(int index);
    Q_INVOKABLE void swapPanes(int first, int second);
    Q_INVOKABLE void removeSeries(QtFinCharts::ChartSeries* series);
    Q_INVOKABLE void resizeChart(qreal width, qreal height);
    Q_INVOKABLE qreal chartWidth() const;
    Q_INVOKABLE qreal chartHeight() const;
    Q_INVOKABLE QVariantMap coordinateToData(const QPointF& point) const;
    Q_INVOKABLE QVariantMap visibleLogicalRange() const;
    Q_INVOKABLE void setVisibleLogicalRange(qreal from, qreal to);
    Q_INVOKABLE QVariantMap visibleTimeRange() const;
    Q_INVOKABLE void setVisibleTimeRange(qint64 from, qint64 to);
    Q_INVOKABLE qreal logicalToCoordinate(qreal logicalIndex) const;
    Q_INVOKABLE qreal coordinateToLogical(qreal x) const;
    Q_INVOKABLE qint64 coordinateToTime(qreal x) const;
    Q_INVOKABLE qreal timeToCoordinate(qint64 time) const;
    Q_INVOKABLE int timeToIndex(qint64 time) const;
    Q_INVOKABLE qint64 indexToTime(int index) const;
    Q_INVOKABLE qreal priceToCoordinate(int pane, double price) const;
    Q_INVOKABLE void setCrosshairPosition(qint64 time, double price, int pane = 0, const QString& priceScaleId = QString());
    Q_INVOKABLE void clearCrosshairPosition();
    Q_INVOKABLE qreal scrollPosition() const;
    Q_INVOKABLE void scrollToPosition(qreal position);
    Q_INVOKABLE void scrollToRealTime();
    Q_INVOKABLE qreal paneHeight(int pane) const;
    Q_INVOKABLE void setPaneHeight(int pane, qreal height);
    Q_INVOKABLE void clearPaneHeight(int pane);
    Q_INVOKABLE qreal paneStretchFactor(int pane) const;
    Q_INVOKABLE void setPaneStretchFactor(int pane, qreal factor);
    Q_INVOKABLE QVariantMap paneSize(int pane) const;
    Q_INVOKABLE QVariantList paneSizes() const;
    Q_INVOKABLE QVariantList panes() const;
    Q_INVOKABLE QVariantList seriesInPane(int pane) const;
    Q_INVOKABLE void moveSeriesToPane(QtFinCharts::ChartSeries* series, int pane, int visualIndex = -1);
    Q_INVOKABLE void moveSeriesInPane(QtFinCharts::ChartSeries* series, int visualIndex);
    Q_INVOKABLE int seriesOrderInPane(QtFinCharts::ChartSeries* series) const;
    Q_INVOKABLE QVariantMap visiblePriceRange(int pane) const;
    Q_INVOKABLE QVariantMap visiblePriceRangeForScale(const QString& priceScaleId, int pane = 0) const;
    Q_INVOKABLE void setVisiblePriceRange(int pane, double minimum, double maximum);
    Q_INVOKABLE void setVisiblePriceRangeForScale(const QString& priceScaleId, int pane, double minimum, double maximum);
    Q_INVOKABLE void clearVisiblePriceRange(int pane = -1);
    Q_INVOKABLE void clearVisiblePriceRangeForScale(const QString& priceScaleId, int pane = -1);
    Q_INVOKABLE void applyOptions(const QVariantMap& options);
    Q_INVOKABLE QVariantMap options() const;
    Q_INVOKABLE bool saveScreenshot(const QString& fileName, const QSize& targetSize = QSize());
    Q_INVOKABLE void remove();
    [[nodiscard]] bool isRemoved() const noexcept { return m_removed; }
    [[nodiscard]] QQuickItem* nativeItem() noexcept { return this; }
    Q_INVOKABLE QObject* chartElement(const QString& name) const;

    void scheduleDataUpdate();
    [[nodiscard]] std::optional<TimePoint> timeAtIndex(int index) const;

Q_SIGNALS:
    void themeChanged();
    void paneCountChanged();
    void gridVisibleChanged();
    void interactiveChanged();
    void interactionOptionsChanged();
    void timeScaleVisibleChanged();
    void axisOptionsChanged();
    void paneSeparatorOptionsChanged();
    void localizationChanged();
    void barSpacingChanged();
    void rightOffsetChanged();
    void crosshairOptionsChanged();
    void visibleLogicalRangeChanged(const QVariantMap& range);
    void visibleTimeRangeChanged(const QVariantMap& range);
    void timeScaleSizeChanged(qreal width, qreal height);
    void clicked(const QVariantMap& payload);
    void doubleClicked(const QVariantMap& payload);
    void crosshairMoved(const QVariantMap& payload);
    void removedChanged();

protected:
    bool event(QEvent* event) override;
    void updatePolish() override;
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData*) override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    void hoverMoveEvent(QHoverEvent* event) override;
    void hoverLeaveEvent(QHoverEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;

private:
    struct PaneAxisTicks {
        QVector<PriceTick> left;
        QVector<PriceTick> right;
    };

    struct RenderState {
        bool valid = false;
        ChartLayout layout;
        int visibleFrom = 0;
        int visibleTo = -1;
        qreal barSpacing = 8.0;
        QVector<TimeTick> timeTicks;
        QVector<PaneAxisTicks> ticksByPane;
        QHash<QString, PriceScale> priceScales;
        QHash<QString, QString> priceLabels;
        QString crosshairTimeLabel;
        QString crosshairPriceLabel;
        QString crosshairPriceScaleId;
        int crosshairPane = -1;
    };

    struct CrosshairResolution {
        QPointF point;
        QString priceScaleId = QStringLiteral("right");
    };

    struct LabelTexture;
    class TextTextureCache;
    class ChartRootNode;
    enum class AxisDragMode {
        None,
        Time,
        Price,
    };

    static void appendSeries(QQmlListProperty<ChartSeries>* property, ChartSeries* series);
    static qsizetype seriesCount(QQmlListProperty<ChartSeries>* property);
    static ChartSeries* seriesAt(QQmlListProperty<ChartSeries>* property, qsizetype index);
    static void clearSeries(QQmlListProperty<ChartSeries>* property);
    [[nodiscard]] static QString normalizedPriceScaleId(const QString& priceScaleId);
    [[nodiscard]] static QString priceScaleKey(int pane, const QString& priceScaleId);

    void attachSeries(ChartSeries* series);
    void detachSeries(ChartSeries* series);
    void handleSeriesDataChanged(const QVariantMap& payload);
    void handleSeriesOptionsChanged();
    [[nodiscard]] bool seriesAttachmentsMatchModel() const;
    void rebuildModel();
    void scheduleVisualUpdate();
    [[nodiscard]] bool syncTimeScaleWithLayout(const ChartLayout& layout);
    void prepareRenderState();
    [[nodiscard]] PriceScale renderPriceScale(int pane, const QString& priceScaleId) const;
    [[nodiscard]] QString renderPriceLabel(double value, PriceFormat format) const;
    void cacheRenderPriceLabel(double value, PriceFormat format);
    [[nodiscard]] static QString priceLabelCacheKey(double value, PriceFormat format);
    [[nodiscard]] static bool layoutContainsPane(const ChartLayout& layout, int pane);
    [[nodiscard]] QString touchPriceScaleIdAt(const QPointF& point, const ChartLayout& layout) const;
    [[nodiscard]] QString defaultCrosshairPriceScaleId(const QPointF& point, const ChartLayout& layout, int paneIndex) const;
    void emitVisibleRangeSignals();
    ChartLayout currentLayoutWithSyncedTimeScale() const;
    void updateCrosshair(const QPointF& point, const QVariantMap& sourceEvent = { });
    bool zoomAtItemPosition(qreal itemX, qreal factor);
    [[nodiscard]] int paneAtY(qreal y, const ChartLayout& layout) const;
    [[nodiscard]] int paneSeparatorAt(const QPointF& point, const ChartLayout& layout) const;
    [[nodiscard]] bool crosshairDrawable() const noexcept;
    [[nodiscard]] CrosshairResolution resolvedCrosshair(const QPointF& point) const;
    [[nodiscard]] QVariantMap hitTestPayload(const QPointF& point) const;
    [[nodiscard]] QVariantMap crosshairPayload(const QPointF& point,
        const QVariantMap& sourceEvent = { },
        const QString& priceScaleId = QString()) const;
    [[nodiscard]] std::optional<PriceRange> autoscaleProviderRange(const ChartSeries* series,
        const PriceRange& defaultRange,
        int pane,
        int visibleFrom,
        int visibleTo) const;
    [[nodiscard]] PriceRange resolvedPriceRange(int pane,
        const QString& priceScaleId,
        int visibleFrom,
        int visibleTo,
        bool* manualRange = nullptr) const;
    void setAutoScaleForScale(const QString& priceScaleId, bool enabled);
    void configurePriceScale(PriceScale& priceScale,
        int pane,
        const QString& priceScaleId,
        int visibleFrom,
        int visibleTo) const;
    [[nodiscard]] std::optional<double> firstVisiblePrice(int pane,
        const QString& priceScaleId,
        int visibleFrom,
        int visibleTo) const;
    [[nodiscard]] bool hasManualPriceRangeForScale(const QString& priceScaleId, int pane = -1) const;
    [[nodiscard]] PriceFormat displayPriceFormat() const noexcept;
    [[nodiscard]] QString formatPriceValue(double value) const;
    [[nodiscard]] QString formatPriceValue(double value, PriceFormat format) const;
    [[nodiscard]] QString formatPriceTickValue(double value) const;
    [[nodiscard]] QString formatPriceTickValue(double value, PriceFormat format) const;
    [[nodiscard]] QString formatTimeValue(TimePoint time) const;
    [[nodiscard]] QString formatTimeTickValue(const TimeTick& tick) const;
    [[nodiscard]] QString callFormatter(const QJSValue& formatter,
        const QJSValueList& args,
        const QString& fallback) const;
    [[nodiscard]] QString activeTimeLabelFormat() const;
    [[nodiscard]] ChartLayout currentLayout() const;

    QVector<ChartSeries*> m_series;
    QHash<ChartSeries*, QPointer<QObject>> m_seriesPreviousParents;
    ChartModel m_model;
    QHash<QString, PriceScale> m_priceScales;
    QHash<QString, PriceRange> m_manualPriceRanges;
    QPointer<ChartTheme> m_theme;
    std::unique_ptr<ChartTheme> m_ownedTheme;
    RenderState m_renderState;
    QLocale m_locale;
    PriceFormat m_priceFormat = PriceFormat::Price;
    QJSValue m_timeFormatter;
    QJSValue m_timeTickMarkFormatter;
    QJSValue m_priceFormatter;
    QJSValue m_percentageFormatter;
    QJSValue m_priceTickMarkFormatter;
    QJSValue m_percentageTickMarkFormatter;

    int m_paneCount = 1;
    int m_pricePrecision = 2;
    bool m_gridVisible = true;
    bool m_verticalGridVisible = true;
    bool m_horizontalGridVisible = true;
    int m_verticalGridLineStyle = static_cast<int>(LineStyle::Solid);
    int m_horizontalGridLineStyle = static_cast<int>(LineStyle::Solid);
    bool m_interactive = true;
    bool m_handleScroll = true;
    bool m_handleScale = true;
    bool m_mouseWheelScroll = false;
    bool m_mouseWheelScale = true;
    bool m_pressedMouseMoveScroll = true;
    bool m_horizontalTouchDrag = true;
    bool m_verticalTouchDrag = true;
    bool m_pinchScale = true;
    bool m_timeScaleVisible = true;
    bool m_priceScaleVisible = true;
    bool m_leftPriceScaleVisible = false;
    bool m_priceAxisBorderVisible = true;
    bool m_leftPriceAxisBorderVisible = true;
    bool m_timeAxisBorderVisible = true;
    bool m_priceAxisTickMarksVisible = false;
    bool m_leftPriceAxisTickMarksVisible = false;
    bool m_timeAxisTickMarksVisible = false;
    bool m_timeVisible = false;
    bool m_secondsVisible = true;
    QString m_dateFormat = QStringLiteral("MMM d");
    QString m_timeFormat = QStringLiteral("MMM d HH:mm");
    QString m_timeWithSecondsFormat = QStringLiteral("MMM d HH:mm:ss");
    qreal m_timeAxisMinimumHeight = 30.0;
    int m_timeAxisMaxLabelWidth = 76;
    bool m_timeAxisUniformDistribution = true;
    bool m_timeAxisBoldLabels = false;
    qreal m_priceAxisMinimumWidth = 60.0;
    qreal m_leftPriceAxisMinimumWidth = 60.0;
    QColor m_priceAxisTextColor;
    QString m_priceAxisLabelAlignment = QStringLiteral("auto");
    qreal m_priceScaleTopMargin = 0.08;
    qreal m_priceScaleBottomMargin = 0.08;
    qreal m_priceTickSpacing = 58.0;
    bool m_autoScale = true;
    bool m_priceScaleInverted = false;
    PriceScaleMode m_priceScaleMode = PriceScaleMode::Normal;
    bool m_preserveEmptyPanes = true;
    bool m_paneSeparatorsResizable = true;
    int m_crosshairMode = static_cast<int>(CrosshairMode::Normal);
    bool m_verticalCrosshairLineVisible = true;
    bool m_horizontalCrosshairLineVisible = true;
    QColor m_verticalCrosshairLineColor;
    QColor m_horizontalCrosshairLineColor;
    qreal m_verticalCrosshairLineWidth = 1.0;
    qreal m_horizontalCrosshairLineWidth = 1.0;
    int m_verticalCrosshairLineStyle = static_cast<int>(LineStyle::Solid);
    int m_horizontalCrosshairLineStyle = static_cast<int>(LineStyle::Solid);
    bool m_timeCrosshairLabelVisible = true;
    bool m_priceCrosshairLabelVisible = true;
    QColor m_timeCrosshairLabelBackground;
    QColor m_priceCrosshairLabelBackground;
    bool m_snapCrosshairToHiddenSeries = false;
    bool m_fitContentRequested = true;
    bool m_textCacheDirty = false;
    bool m_dragging = false;
    bool m_dragMoved = false;
    bool m_crosshairVisible = false;
    bool m_removed = false;
    QPointF m_crosshairPosition;
    QString m_crosshairPriceScaleId = QStringLiteral("right");
    QPointF m_pressPosition;
    QPointF m_lastTouchPosition;
    qreal m_lastDragX = 0.0;
    AxisDragMode m_axisDragMode = AxisDragMode::None;
    int m_axisDragPane = -1;
    QString m_axisDragPriceScaleId = QStringLiteral("right");
    qreal m_axisDragStartBarSpacing = 8.0;
    PriceRange m_axisDragStartRange;
    double m_axisDragAnchorPrice = std::numeric_limits<double>::quiet_NaN();
    int m_hoveredPaneSeparator = -1;
    int m_draggedPaneSeparator = -1;
    bool m_touchDragging = false;
    int m_touchDragPane = -1;
    QString m_touchDragPriceScaleId = QStringLiteral("right");
    qreal m_separatorDragUpperHeight = 0.0;
    qreal m_separatorDragLowerHeight = 0.0;
};

} // namespace QtFinCharts
