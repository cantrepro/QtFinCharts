import QtQuick
import QtFinCharts

Window {
    id: root
    width: 1360
    height: 900
    visible: true
    title: "QtFinCharts Showcase"
    color: darkMode ? "#101216" : "#eef1f5"

    property bool darkMode: false
    property bool liveUpdates: false
    property bool rangeVisible: true
    property bool compactAxes: false
    property bool pinnedCrosshair: false
    property int focusIndex: 70
    property int liveTick: 0
    property string statusText: "Ready"
    property string rangeText: "Range -"
    property string dataText: "Data -"
    property string geometryText: "Geometry -"
    property string layoutText: "Layout -"
    property string formatterText: "Formatters -"
    property var marketCandles: []
    property var marketAverage: []
    property var marketVolume: []
    property var marketOverlayVolume: []
    property var ohlcRows: []
    property var areaRows: []
    property var baselineRows: []
    property var breadthRows: []
    property var apiLineRows: []
    property var apiHistogramRows: []
    property var strategyRows: []
    property var strategyBandRows: []
    property var strategyExposureRows: []
    property var sessionRows: []
    property var sessionAverageRows: []
    property var sessionEventRows: []
    property int sessionGapCount: 0

    readonly property var activeTheme: darkMode ? darkTheme : lightTheme
    readonly property color panelColor: darkMode ? "#171b22" : "#ffffff"
    readonly property color edgeColor: darkMode ? "#2b313c" : "#d8dee8"
    readonly property color textColor: darkMode ? "#e8edf5" : "#1c2430"
    readonly property color mutedTextColor: darkMode ? "#98a4b5" : "#667386"
    readonly property color controlColor: darkMode ? "#222936" : "#f8fafc"
    readonly property color pressedControlColor: darkMode ? "#2e3745" : "#e8edf4"

    ChartTheme {
        id: lightTheme
        backgroundColor: "#ffffff"
        textColor: "#293241"
        gridColor: "#e0e6ef"
        priceAxisBorderColor: "#b7c0ce"
        timeAxisBorderColor: "#c6ceda"
        crosshairColor: "#49566a"
        crosshairLabelBackground: "#2062d8"
        crosshairLabelText: "#ffffff"
        paneSeparatorColor: "#d9e0ea"
        fontFamily: "Inter"
        fontPixelSize: 11
    }

    ChartTheme {
        id: darkTheme
        backgroundType: "verticalGradient"
        backgroundTopColor: "#171c24"
        backgroundBottomColor: "#111419"
        textColor: "#d9e0ea"
        gridColor: "#29303b"
        priceAxisBorderColor: "#414958"
        timeAxisBorderColor: "#343c49"
        crosshairColor: "#a4afbf"
        crosshairLabelBackground: "#3d6fe6"
        crosshairLabelText: "#ffffff"
        paneSeparatorColor: "#252b34"
        fontFamily: "Inter"
        fontPixelSize: 11
    }

    component PillButton: Rectangle {
        id: control
        property string label: ""
        property color surfaceColor: "#ffffff"
        property color pressedColor: "#e8edf4"
        property color edgeColor: "#d8dee8"
        property color labelColor: "#1c2430"
        signal triggered()

        width: Math.max(72, labelText.implicitWidth + 28)
        height: 32
        radius: 6
        color: mouse.pressed ? pressedColor : surfaceColor
        border.color: edgeColor
        border.width: 1

        Text {
            id: labelText
            anchors.centerIn: parent
            text: control.label
            color: control.labelColor
            font.pixelSize: 12
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        MouseArea {
            id: mouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: control.triggered()
        }
    }

    component ChartCard: Rectangle {
        id: card
        property string heading: ""
        property string detail: ""
        property color surfaceColor: "#ffffff"
        property color edgeColor: "#d8dee8"
        property color headingColor: "#1c2430"
        property color detailColor: "#667386"
        default property alias content: body.data

        radius: 8
        color: surfaceColor
        border.color: edgeColor
        border.width: 1
        clip: true

        Text {
            id: titleText
            x: 16
            y: 11
            width: parent.width - 32
            text: card.heading
            color: card.headingColor
            font.pixelSize: 15
            font.weight: Font.DemiBold
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        Text {
            x: 16
            y: 31
            width: parent.width - 32
            text: card.detail
            color: card.detailColor
            font.pixelSize: 11
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        Item {
            id: body
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 54
            anchors.leftMargin: 1
            anchors.rightMargin: 1
            anchors.bottomMargin: 1
            clip: true
        }
    }

    Rectangle {
        id: header
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: 64
        color: root.panelColor
        border.color: root.edgeColor

        Text {
            id: title
            anchors.left: parent.left
            anchors.leftMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            text: "QtFinCharts Showcase"
            color: root.textColor
            font.pixelSize: 20
            font.weight: Font.DemiBold
        }

        Row {
            anchors.right: parent.right
            anchors.rightMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8

            PillButton {
                label: "Fit"
                surfaceColor: root.controlColor
                pressedColor: root.pressedControlColor
                edgeColor: root.edgeColor
                labelColor: root.textColor
                onTriggered: root.fitAll()
            }
            PillButton {
                label: "Window"
                surfaceColor: root.controlColor
                pressedColor: root.pressedControlColor
                edgeColor: root.edgeColor
                labelColor: root.textColor
                onTriggered: root.zoomWindow()
            }
            PillButton {
                label: root.pinnedCrosshair ? "Clear" : "Pin"
                surfaceColor: root.controlColor
                pressedColor: root.pressedControlColor
                edgeColor: root.edgeColor
                labelColor: root.textColor
                onTriggered: root.togglePinnedCrosshairs()
            }
            PillButton {
                label: "Right"
                surfaceColor: root.controlColor
                pressedColor: root.pressedControlColor
                edgeColor: root.edgeColor
                labelColor: root.textColor
                onTriggered: root.scrollAllToRealtime()
            }
            PillButton {
                label: root.compactAxes ? "Axes" : "Compact"
                surfaceColor: root.controlColor
                pressedColor: root.pressedControlColor
                edgeColor: root.edgeColor
                labelColor: root.textColor
                onTriggered: {
                    root.compactAxes = !root.compactAxes
                    root.refreshStats()
                }
            }
            PillButton {
                label: root.rangeVisible ? "Hide Range" : "Show Range"
                width: Math.max(100, label.length * 8 + 28)
                surfaceColor: root.controlColor
                pressedColor: root.pressedControlColor
                edgeColor: root.edgeColor
                labelColor: root.textColor
                onTriggered: {
                    root.rangeVisible = !root.rangeVisible
                    root.refreshStats()
                }
            }
            PillButton {
                label: root.liveUpdates ? "Pause" : "Live"
                surfaceColor: root.controlColor
                pressedColor: root.pressedControlColor
                edgeColor: root.edgeColor
                labelColor: root.textColor
                onTriggered: root.liveUpdates = !root.liveUpdates
            }
            PillButton {
                label: root.darkMode ? "Light" : "Dark"
                surfaceColor: root.controlColor
                pressedColor: root.pressedControlColor
                edgeColor: root.edgeColor
                labelColor: root.textColor
                onTriggered: {
                    root.darkMode = !root.darkMode
                    root.refreshStats()
                }
            }
        }
    }

    Flickable {
        id: viewport
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: header.bottom
        anchors.bottom: footer.top
        contentWidth: width
        contentHeight: chartGrid.implicitHeight + 36
        clip: true

        Grid {
            id: chartGrid
            x: 18
            y: 18
            width: viewport.width - 36
            columns: root.width < 1050 ? 1 : 2
            spacing: 14

            ChartCard {
                width: (chartGrid.width - chartGrid.spacing * (chartGrid.columns - 1)) / chartGrid.columns
                height: root.width < 1050 ? 350 : 360
                heading: "Price / Volume"
                detail: root.rangeText
                surfaceColor: root.panelColor
                edgeColor: root.edgeColor
                headingColor: root.textColor
                detailColor: root.mutedTextColor

                FinancialChart {
                    id: marketChart
                    anchors.fill: parent
                    theme: root.activeTheme
                    paneCount: 2
                    localeName: "en_US"
                    pricePrecision: 2
                    priceFormat: "price"
                    timeVisible: false
                    secondsVisible: false
                    dateFormat: "MMM d"
                    timeScaleVisible: true
                    priceScaleVisible: true
                    priceAxisTickMarksVisible: true
                    timeAxisTickMarksVisible: true
                    timeAxisMinimumHeight: 30
                    priceAxisMinimumWidth: 78
                    interactive: true
                    handleScroll: true
                    handleScale: true
                    mouseWheelScroll: true
                    mouseWheelScale: true
                    pressedMouseMoveScroll: true
                    pinchScale: true
                    barSpacing: 7
                    rightOffset: 4
                    minBarSpacing: 2
                    maxBarSpacing: 28
                    verticalGridLineStyle: 2
                    horizontalGridLineStyle: 1
                    onCrosshairMoved: function(payload) { root.describePayload("Price", payload) }
                    onClicked: function(payload) { root.describeClick("Price", payload) }
                    onDoubleClicked: function(payload) {
                        root.describeClick("Price double", payload)
                        marketChart.resetTimeScale()
                    }
                    onVisibleLogicalRangeChanged: function(range) { root.updateRangeText(range) }
                    onVisibleTimeRangeChanged: function(range) { root.updateTimeRangeText(range) }
                    onTimeScaleSizeChanged: function(width, height) { root.geometryText = "Market axis " + Math.round(width) + "x" + Math.round(height) }

                    HistogramSeries {
                        id: overlayVolumeBars
                        name: "Overlay volume"
                        priceScaleId: "overlay-volume"
                        color: "#335c7ec4"
                        baseValue: 0
                    }

                    CandlestickSeries {
                        id: priceCandles
                        title: "Price"
                        upColor: "#1fa187"
                        downColor: "#d95262"
                        borderUpColor: "#16856f"
                        borderDownColor: "#b53f4d"
                        wickUpColor: "#16856f"
                        wickDownColor: "#b53f4d"
                        borderVisible: true
                        wickVisible: true
                        onDataChangedDetailed: function(payload) { root.dataText = root.payloadText(payload) }
                    }

                    LineSeries {
                        id: emaLine
                        name: "EMA"
                        color: "#d7a72b"
                        lineWidth: 2
                        lineStyle: 2
                        lineType: 2
                        pointMarkersVisible: true
                        pointMarkersRadius: 2.1
                        crosshairMarkerRadius: 5
                        crosshairMarkerBorderColor: "#ffffff"
                        crosshairMarkerBorderWidth: 2
                        crosshairMarkerBackgroundColor: "#d7a72b"
                    }

                    HistogramSeries {
                        id: volumeBars
                        name: "Volume"
                        pane: 1
                        color: "#5c7ec4"
                        baseValue: 0
                    }
                }
            }

            ChartCard {
                width: (chartGrid.width - chartGrid.spacing * (chartGrid.columns - 1)) / chartGrid.columns
                height: root.width < 1050 ? 350 : 360
                heading: "OHLC / Range"
                detail: root.rangeVisible ? "Range visible" : "OHLC only"
                surfaceColor: root.panelColor
                edgeColor: root.edgeColor
                headingColor: root.textColor
                detailColor: root.mutedTextColor

                FinancialChart {
                    id: ohlcChart
                    anchors.fill: parent
                    theme: root.activeTheme
                    paneCount: 1
                    localeName: "en_GB"
                    pricePrecision: 2
                    priceFormat: "price"
                    timeVisible: false
                    dateFormat: "dd MMM"
                    timeAxisBorderVisible: true
                    priceAxisBorderVisible: false
                    priceAxisTickMarksVisible: false
                    timeAxisTickMarksVisible: true
                    timeAxisMinimumHeight: 28
                    priceAxisMinimumWidth: 72
                    handleScroll: true
                    handleScale: true
                    mouseWheelScroll: false
                    mouseWheelScale: true
                    pressedMouseMoveScroll: true
                    pinchScale: true
                    rightOffsetPixels: 42
                    barSpacing: 6
                    minBarSpacing: 2
                    maxBarSpacing: 22
                    verticalGridVisible: true
                    horizontalGridVisible: true
                    verticalGridLineStyle: 4
                    horizontalGridLineStyle: 0
                    onCrosshairMoved: function(payload) { root.describePayload("OHLC", payload) }
                    onClicked: function(payload) { root.describeClick("OHLC", payload) }

                    BarSeries {
                        id: ohlcBars
                        name: "OHLC"
                        upColor: "#2a9d8f"
                        downColor: "#c4455c"
                        openVisible: true
                        thinBars: false
                    }

                    AreaSeries {
                        id: rangeArea
                        name: "Range"
                        visible: root.rangeVisible
                        topColor: "#627ac633"
                        bottomColor: "#1b7a8c0e"
                        relativeGradient: true
                        invertFilledArea: false
                        lineColor: "#4377c7"
                        lineWidth: 2
                        lineStyle: 4
                        lineType: 1
                        pointMarkersVisible: true
                        pointMarkersRadius: 1.8
                        crosshairMarkerRadius: 4
                        crosshairMarkerBorderColor: "#ffffff"
                        crosshairMarkerBackgroundColor: "#4377c7"
                    }
                }
            }

            ChartCard {
                width: (chartGrid.width - chartGrid.spacing * (chartGrid.columns - 1)) / chartGrid.columns
                height: root.width < 1050 ? 350 : 360
                heading: "Return / Breadth"
                detail: root.compactAxes ? "Compact axes" : "Percent scale"
                surfaceColor: root.panelColor
                edgeColor: root.edgeColor
                headingColor: root.textColor
                detailColor: root.mutedTextColor

                FinancialChart {
                    id: percentChart
                    anchors.fill: parent
                    theme: root.activeTheme
                    paneCount: 2
                    localeName: "de_DE"
                    pricePrecision: 2
                    priceFormat: "percent"
                    timeVisible: false
                    dateFormat: "dd.MM"
                    timeScaleVisible: !root.compactAxes
                    priceScaleVisible: true
                    priceAxisTickMarksVisible: true
                    timeAxisTickMarksVisible: !root.compactAxes
                    timeAxisUniformDistribution: true
                    timeAxisBoldLabels: true
                    timeAxisMinimumHeight: 32
                    priceAxisMinimumWidth: 82
                    barSpacing: 6.5
                    rightOffset: 3
                    minBarSpacing: 2
                    maxBarSpacing: 24
                    gridVisible: true
                    verticalGridLineStyle: 1
                    horizontalGridLineStyle: 2
                    onCrosshairMoved: function(payload) { root.describePayload("Return", payload) }
                    onClicked: function(payload) { root.describeClick("Return", payload) }

                    BaselineSeries {
                        id: returnBaseline
                        name: "Return"
                        baseValue: 0
                        relativeGradient: true
                        topFillColor1: "#339b7d66"
                        topFillColor2: "#339b7d11"
                        bottomFillColor1: "#c44b5a11"
                        bottomFillColor2: "#c44b5a66"
                        topLineColor: "#248b72"
                        bottomLineColor: "#c44b5a"
                        lineWidth: 2.4
                        lineStyle: 0
                        lineType: 2
                        pointMarkersVisible: false
                        crosshairMarkerRadius: 5
                        crosshairMarkerBackgroundColor: "#248b72"
                    }

                    HistogramSeries {
                        id: breadthBars
                        name: "Breadth"
                        pane: 1
                        color: "#7b6fc5"
                        baseValue: 50
                    }
                }
            }

            ChartCard {
                width: (chartGrid.width - chartGrid.spacing * (chartGrid.columns - 1)) / chartGrid.columns
                height: root.width < 1050 ? 350 : 360
                heading: "Data APIs / Flow"
                detail: root.geometryText
                surfaceColor: root.panelColor
                edgeColor: root.edgeColor
                headingColor: root.textColor
                detailColor: root.mutedTextColor

                FinancialChart {
                    id: apiChart
                    anchors.fill: parent
                    theme: root.activeTheme
                    paneCount: 2
                    localeName: "en_US"
                    pricePrecision: 1
                    priceFormat: "volume"
                    timeVisible: true
                    secondsVisible: true
                    dateFormat: "MMM d"
                    timeFormat: "HH:mm"
                    timeWithSecondsFormat: "HH:mm:ss"
                    timeScaleVisible: true
                    priceScaleVisible: !root.compactAxes
                    priceAxisTickMarksVisible: true
                    timeAxisTickMarksVisible: true
                    timeAxisMinimumHeight: 32
                    timeAxisMaxLabelWidth: 92
                    priceAxisMinimumWidth: 88
                    handleScroll: true
                    handleScale: true
                    mouseWheelScroll: true
                    mouseWheelScale: true
                    pressedMouseMoveScroll: true
                    pinchScale: true
                    barSpacing: 8
                    rightOffset: 6
                    minBarSpacing: 2
                    maxBarSpacing: 30
                    gridVisible: true
                    verticalGridLineStyle: 0
                    horizontalGridLineStyle: 4
                    onCrosshairMoved: function(payload) { root.describePayload("Flow", payload) }
                    onClicked: function(payload) { root.describeClick("Flow", payload) }

                    LineSeries {
                        id: flowLine
                        name: "Flow"
                        pane: 0
                        color: "#2d7dd2"
                        lineWidth: 2.2
                        lineStyle: 0
                        lineType: 2
                        pointMarkersVisible: true
                        pointMarkersRadius: 2
                        crosshairMarkerRadius: 5
                        crosshairMarkerBorderColor: "#ffffff"
                        crosshairMarkerBackgroundColor: "#2d7dd2"
                        onDataChangedDetailed: function(payload) { root.dataText = root.payloadText(payload) }
                    }

                    HistogramSeries {
                        id: flowBars
                        name: "Flow bars"
                        pane: 1
                        color: "#8a7ad3"
                        baseValue: 0
                    }

                    LineSeries {
                        id: temporaryPaneLine
                        name: "Temporary"
                        pane: 1
                        color: "#b56576"
                        lineWidth: 1.5
                        lineStyle: 3
                        lineType: 1
                        visible: true
                    }
                }
            }

            ChartCard {
                width: (chartGrid.width - chartGrid.spacing * (chartGrid.columns - 1)) / chartGrid.columns
                height: root.width < 1050 ? 350 : 360
                heading: "Price Lines / Range"
                detail: root.layoutText
                surfaceColor: root.panelColor
                edgeColor: root.edgeColor
                headingColor: root.textColor
                detailColor: root.mutedTextColor

                FinancialChart {
                    id: strategyChart
                    anchors.fill: parent
                    theme: root.activeTheme
                    paneCount: 2
                    localeName: "en_US"
                    pricePrecision: 2
                    priceFormat: "price"
                    timeVisible: false
                    secondsVisible: false
                    dateFormat: "MMM d"
                    timeScaleVisible: true
                    priceScaleVisible: true
                    priceAxisTickMarksVisible: true
                    timeAxisTickMarksVisible: true
                    timeAxisMinimumHeight: 30
                    priceAxisMinimumWidth: 82
                    priceScaleTopMargin: 0.16
                    priceScaleBottomMargin: 0.12
                    barSpacing: 7
                    rightOffset: 5
                    minBarSpacing: 2
                    maxBarSpacing: 24
                    verticalGridLineStyle: 3
                    horizontalGridLineStyle: 2
                    crosshairMode: 1
                    verticalCrosshairLineStyle: 2
                    horizontalCrosshairLineStyle: 4
                    onCrosshairMoved: function(payload) { root.describePayload("Strategy", payload) }
                    onClicked: function(payload) { root.describeClick("Strategy", payload) }

                    AreaSeries {
                        id: strategyBand
                        name: "Risk band"
                        pane: 0
                        topColor: "#4f8f7244"
                        bottomColor: "#4f8f7208"
                        relativeGradient: true
                        invertFilledArea: false
                        lineColor: "#4f8f72"
                        lineWidth: 1.5
                        lineStyle: 3
                        lineType: 1
                        pointMarkersVisible: false
                        crosshairMarkerVisible: false
                    }

                    LineSeries {
                        id: strategyLine
                        name: "Strategy"
                        pane: 0
                        color: "#2f6f8f"
                        lineWidth: 2.4
                        lineStyle: 0
                        lineType: 2
                        pointMarkersVisible: true
                        pointMarkersRadius: 1.7
                        lastValueLabelVisible: true
                        priceLineVisible: true
                        priceLineSource: 1
                        priceLineColor: "#2f6f8f"
                        priceLineWidth: 1.2
                        priceLineStyle: 2
                        baseLineVisible: true
                        baseLineColor: "#7b6fc5"
                        baseLineWidth: 1
                        baseLineStyle: 4
                        hitTestTolerance: 8
                        crosshairMarkerRadius: 5
                        crosshairMarkerBorderColor: "#ffffff"
                        crosshairMarkerBackgroundColor: "#2f6f8f"
                    }

                    HistogramSeries {
                        id: strategyExposure
                        name: "Exposure"
                        pane: 1
                        color: "#c28b3c"
                        baseValue: 0
                    }
                }
            }

            ChartCard {
                width: (chartGrid.width - chartGrid.spacing * (chartGrid.columns - 1)) / chartGrid.columns
                height: root.width < 1050 ? 350 : 360
                heading: "Formatters / Gaps"
                detail: root.formatterText
                surfaceColor: root.panelColor
                edgeColor: root.edgeColor
                headingColor: root.textColor
                detailColor: root.mutedTextColor

                FinancialChart {
                    id: sessionChart
                    anchors.fill: parent
                    theme: root.activeTheme
                    paneCount: 2
                    localeName: "en_US"
                    pricePrecision: 0
                    priceFormat: "volume"
                    timeVisible: true
                    secondsVisible: false
                    dateFormat: "MMM d"
                    timeFormat: "HH:mm"
                    timeScaleVisible: true
                    priceScaleVisible: !root.compactAxes
                    priceAxisTickMarksVisible: true
                    timeAxisTickMarksVisible: true
                    timeAxisMinimumHeight: 34
                    timeAxisMaxLabelWidth: 84
                    priceAxisMinimumWidth: 86
                    barSpacing: 8.5
                    rightOffset: 4
                    minBarSpacing: 2
                    maxBarSpacing: 32
                    fixLeftEdge: true
                    rightBarStaysOnScroll: true
                    verticalGridLineStyle: 1
                    horizontalGridLineStyle: 3
                    crosshairMode: 2
                    timeFormatter: function(seconds) { return root.formatSessionTime(seconds, true) }
                    timeTickMarkFormatter: function(seconds, index) { return index % 4 === 0 ? root.formatSessionTime(seconds, false) : "" }
                    priceFormatter: function(value) { return root.compactNumber(value, 1) }
                    priceTickMarkFormatter: function(value) { return root.compactNumber(value, 0) }
                    onCrosshairMoved: function(payload) { root.describePayload("Session", payload) }
                    onClicked: function(payload) { root.describeClick("Session", payload) }

                    LineSeries {
                        id: sessionLine
                        name: "Liquidity"
                        pane: 0
                        color: "#3b6ea8"
                        lineWidth: 2.2
                        lineStyle: 0
                        lineType: 1
                        pointMarkersVisible: true
                        pointMarkersRadius: 1.8
                        crosshairMarkerRadius: 5
                        crosshairMarkerBorderColor: "#ffffff"
                        crosshairMarkerBackgroundColor: "#3b6ea8"
                        onDataChangedDetailed: function(payload) { root.formatterText = root.payloadText(payload) }
                    }

                    LineSeries {
                        id: sessionAverage
                        name: "Median"
                        pane: 0
                        color: "#b36b42"
                        lineWidth: 1.6
                        lineStyle: 2
                        lineType: 0
                        pointMarkersVisible: false
                        priceLineVisible: true
                        priceLineColor: "#b36b42"
                        priceLineWidth: 1
                        priceLineStyle: 2
                        crosshairMarkerVisible: false
                    }

                    HistogramSeries {
                        id: sessionEvents
                        name: "Events"
                        pane: 1
                        color: "#6e8f5b"
                        baseValue: 0
                    }
                }
            }
        }
    }

    Rectangle {
        id: footer
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 38
        color: root.panelColor
        border.color: root.edgeColor

        Text {
            anchors.left: parent.left
            anchors.leftMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width * 0.36
            text: root.statusText
            color: root.textColor
            font.pixelSize: 12
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        Text {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width * 0.28
            horizontalAlignment: Text.AlignHCenter
            text: root.dataText
            color: root.mutedTextColor
            font.pixelSize: 12
            elide: Text.ElideRight
            maximumLineCount: 1
        }

        Text {
            anchors.right: parent.right
            anchors.rightMargin: 18
            anchors.verticalCenter: parent.verticalCenter
            width: parent.width * 0.30
            horizontalAlignment: Text.AlignRight
            text: root.geometryText
            color: root.mutedTextColor
            font.pixelSize: 12
            elide: Text.ElideRight
            maximumLineCount: 1
        }
    }

    Timer {
        id: liveTimer
        interval: 1200
        repeat: true
        running: root.liveUpdates
        onTriggered: root.appendLivePoint()
    }

    Timer {
        id: deferredSetup
        interval: 0
        repeat: false
        onTriggered: {
            root.fitAll()
            root.togglePinnedCrosshairs()
            root.refreshStats()
        }
    }

    Component.onCompleted: {
        generateData()
        configureCharts()
        loadData()
        deferredSetup.start()
    }

    function formatNumber(value, digits) {
        return isFinite(value) ? Number(value).toFixed(digits) : "-"
    }

    function formatDate(seconds) {
        if (seconds === undefined || !isFinite(seconds)) {
            return "-"
        }
        var d = new Date(seconds * 1000)
        return (d.getUTCMonth() + 1) + "/" + d.getUTCDate()
    }

    function formatSessionTime(seconds, withZone) {
        if (seconds === undefined || !isFinite(seconds)) {
            return "-"
        }
        var d = new Date(seconds * 1000)
        var hours = d.getUTCHours()
        var minutes = d.getUTCMinutes()
        var label = (hours < 10 ? "0" : "") + hours + ":" + (minutes < 10 ? "0" : "") + minutes
        return withZone ? label + " UTC" : label
    }

    function compactNumber(value, digits) {
        if (value === undefined || !isFinite(value)) {
            return "-"
        }
        var sign = value < 0 ? "-" : ""
        var absolute = Math.abs(value)
        if (absolute >= 1000000) {
            return sign + Number(absolute / 1000000).toFixed(digits) + "M"
        }
        if (absolute >= 1000) {
            return sign + Number(absolute / 1000).toFixed(digits) + "K"
        }
        return sign + Number(absolute).toFixed(digits)
    }

    function nearestRowWithValue(rows, index, fallback) {
        if (!rows || rows.length === 0) {
            return { time: 0, value: fallback }
        }
        var clamped = Math.max(0, Math.min(rows.length - 1, index))
        for (var step = 0; step < rows.length; ++step) {
            var left = clamped - step
            if (left >= 0 && rows[left].value !== undefined) {
                return rows[left]
            }
            var right = clamped + step
            if (right < rows.length && rows[right].value !== undefined) {
                return rows[right]
            }
        }
        return { time: rows[clamped].time, value: fallback }
    }

    function payloadText(payload) {
        var seriesName = payload.series || payload.type || "series"
        var scope = payload.scope || "full"
        var count = payload.count !== undefined ? " " + payload.count : ""
        return seriesName + " " + scope + count
    }

    function describePayload(name, payload) {
        if (!payload || payload.time === undefined) {
            return
        }
        root.statusText = name + " " + (payload.timeLabel || formatDate(payload.time)) + " " + (payload.priceLabel || formatNumber(payload.price, 2))
    }

    function describeClick(name, payload) {
        if (!payload || payload.time === undefined) {
            root.statusText = name + " click"
            return
        }
        var row = payload.seriesData && payload.seriesData.Price ? payload.seriesData.Price : ({})
        var close = row.close !== undefined ? " close " + formatNumber(row.close, 2) : ""
        root.statusText = name + " " + (payload.timeLabel || formatDate(payload.time)) + close
    }

    function updateRangeText(range) {
        if (!range || range.from === undefined) {
            return
        }
        root.rangeText = "Logical " + formatNumber(range.from, 1) + " - " + formatNumber(range.to, 1)
    }

    function updateTimeRangeText(range) {
        if (!range || range.from === undefined) {
            return
        }
        root.rangeText = root.rangeText + " | " + formatDate(range.from) + " - " + formatDate(range.to)
    }

    function generateData() {
        var candles = []
        var averages = []
        var volumes = []
        var overlayVolumes = []
        var ohlc = []
        var areas = []
        var baseline = []
        var breadth = []
        var strategy = []
        var strategyBand = []
        var strategyExposure = []
        var day0 = Date.UTC(2024, 0, 2) / 1000
        var previous = 128.0
        var ema = previous

        for (var i = 0; i < 150; ++i) {
            var time = day0 + i * 24 * 60 * 60
            var trend = i * 0.055
            var wave = Math.sin(i / 6.0) * 2.8 + Math.cos(i / 19.0) * 3.2
            var open = previous
            var close = 128.0 + trend + wave + Math.sin(i / 2.4) * 0.65
            var high = Math.max(open, close) + 1.05 + (i % 6) * 0.16
            var low = Math.min(open, close) - 0.95 - (i % 7) * 0.12
            var candle = { time: time, open: open, high: high, low: low, close: close }
            candles.push(candle)
            ohlc.push(candle)

            ema = i === 0 ? close : ema * 0.84 + close * 0.16
            averages.push({ time: time, value: ema })
            areas.push({ time: time, value: low + (high - low) * 0.42 })

            var volume = 820000 + Math.abs(close - open) * 260000 + (i % 10) * 42000
            volumes.push({ time: time, value: volume, color: close >= open ? "#2a9d8f" : "#c4455c" })
            overlayVolumes.push({ time: time, value: volume, color: close >= open ? "#442a9d8f" : "#44c4455c" })

            var percent = (close - 128.0) / 128.0 * 100.0
            baseline.push({ time: time, value: percent })

            var breadthValue = 52 + Math.sin(i / 8.0) * 20 + Math.cos(i / 17.0) * 12
            breadth.push({
                time: time,
                value: breadthValue,
                color: breadthValue >= 50 ? "#339b7d" : "#c44b5a"
            })
            previous = close
        }

        var strategyStart = Date.UTC(2024, 2, 4) / 1000
        for (var s = 0; s < 132; ++s) {
            var strategyTime = strategyStart + s * 24 * 60 * 60
            var drift = s * 0.035
            var strategyValue = 100 + drift + Math.sin(s / 5.5) * 2.3 + Math.cos(s / 18.0) * 3.1
            var bandValue = strategyValue - 2.6 - Math.abs(Math.sin(s / 8.0)) * 2.4
            var exposure = 42 + Math.sin(s / 6.5) * 24 + Math.cos(s / 20.0) * 14
            strategy.push({
                time: strategyTime,
                value: strategyValue,
                color: strategyValue >= 100 ? "#2f6f8f" : "#c4455c"
            })
            strategyBand.push({
                time: strategyTime,
                value: bandValue,
                lineColor: bandValue < 96 ? "#c4455c" : "#4f8f72",
                topColor: bandValue < 96 ? "#c4455c33" : "#4f8f7244",
                bottomColor: bandValue < 96 ? "#c4455c08" : "#4f8f7208"
            })
            strategyExposure.push({
                time: strategyTime,
                value: exposure,
                color: exposure >= 50 ? "#6e8f5b" : "#c28b3c"
            })
        }

        var flow = []
        var flowBarsData = []
        var start = Date.UTC(2024, 1, 5, 9, 30, 0) / 1000
        for (var j = 0; j < 96; ++j) {
            var flowTime = start + j * 15 * 60
            var impulse = Math.sin(j / 5.0) * 42000 + Math.cos(j / 13.0) * 30000 + j * 950
            var flowValue = 460000 + impulse
            if (j % 17 === 0) {
                flow.push({ time: flowTime })
            } else {
                flow.push({ time: flowTime, value: flowValue })
            }
            var barValue = 120000 + Math.abs(impulse) * 1.8 + (j % 9) * 9000
            flowBarsData.push({
                time: flowTime,
                value: barValue,
                color: impulse >= 0 ? "#2a9d8f" : "#c4455c"
            })
        }

        var session = []
        var sessionAverage = []
        var sessionEvents = []
        var gaps = 0
        var sessionStart = Date.UTC(2024, 3, 8, 13, 30, 0) / 1000
        var sessionMean = 320000
        for (var q = 0; q < 84; ++q) {
            var sessionTime = sessionStart + q * 10 * 60
            var sessionImpulse = Math.sin(q / 4.5) * 68000 + Math.cos(q / 14.0) * 48000
            var liquidity = 320000 + sessionImpulse + q * 1100
            if (q % 19 === 0 || q === 47) {
                session.push({ time: sessionTime })
                ++gaps
            } else {
                session.push({ time: sessionTime, value: liquidity, color: liquidity >= 320000 ? "#3b6ea8" : "#b36b42" })
                sessionMean = sessionMean * 0.9 + liquidity * 0.1
            }
            sessionAverage.push({ time: sessionTime, value: sessionMean })
            var eventValue = Math.abs(sessionImpulse) * 0.45 + (q % 7) * 7500
            sessionEvents.push({
                time: sessionTime,
                value: eventValue,
                color: q % 19 === 0 || q === 47 ? "#c4455c" : "#6e8f5b"
            })
        }

        root.marketCandles = candles
        root.marketAverage = averages
        root.marketVolume = volumes
        root.marketOverlayVolume = overlayVolumes
        root.ohlcRows = ohlc
        root.areaRows = areas
        root.baselineRows = baseline
        root.breadthRows = breadth
        root.apiLineRows = flow
        root.apiHistogramRows = flowBarsData
        root.strategyRows = strategy
        root.strategyBandRows = strategyBand
        root.strategyExposureRows = strategyExposure
        root.sessionRows = session
        root.sessionAverageRows = sessionAverage
        root.sessionEventRows = sessionEvents
        root.sessionGapCount = gaps
    }

    function configureCharts() {
        marketChart.applyOptions({
            handleScroll: { enabled: true, mouseWheel: true, pressedMouseMove: true },
            handleScale: { enabled: true, mouseWheel: true, pinch: true },
            grid: { visible: true, verticalVisible: true, horizontalVisible: true, verticalLineStyle: 2, horizontalLineStyle: 1 },
            timeScale: { barSpacing: 7, rightOffset: 4, minBarSpacing: 2, maxBarSpacing: 28, minimumHeight: 30, maxLabelWidth: 92 },
            rightPriceScale: { visible: true, borderVisible: true, tickMarksVisible: true, minimumWidth: 78 },
            localization: { localeName: "en_US", pricePrecision: 2, priceFormat: "price", timeVisible: false, secondsVisible: false }
        })
        ohlcChart.applyOptions({
            handleScroll: { enabled: true, mouseWheel: false, pressedMouseMove: true },
            handleScale: { enabled: true, mouseWheel: true, pinch: true },
            timeScale: { rightOffsetPixels: 42, minBarSpacing: 2, maxBarSpacing: 22, minimumHeight: 28 },
            rightPriceScale: { visible: true, borderVisible: false, tickMarksVisible: false, minimumWidth: 72 }
        })
        percentChart.applyOptions({
            grid: { visible: true, verticalVisible: true, horizontalVisible: true, verticalLineStyle: 1, horizontalLineStyle: 2 },
            timeScale: { barSpacing: 6.5, rightOffset: 3, minBarSpacing: 2, maxBarSpacing: 24, uniformDistribution: true, boldLabels: true },
            localization: { localeName: "de_DE", pricePrecision: 2, priceFormat: "percent" }
        })
        apiChart.applyOptions({
            handleScroll: { enabled: true, mouseWheel: true, pressedMouseMove: true },
            handleScale: { enabled: true, mouseWheel: true, pinch: true },
            grid: { visible: true, verticalVisible: true, horizontalVisible: true, lineStyle: 4 },
            timeScale: { barSpacing: 8, rightOffset: 6, minBarSpacing: 2, maxBarSpacing: 30, minimumHeight: 32, maxLabelWidth: 92 },
            localization: { localeName: "en_US", pricePrecision: 1, priceFormat: "volume", timeVisible: true, secondsVisible: true }
        })
        strategyChart.applyOptions({
            handleScroll: { enabled: true, mouseWheel: true, pressedMouseMove: true },
            handleScale: { enabled: true, mouseWheel: true, pinch: true },
            rightPriceScale: { visible: true, borderVisible: true, tickMarksVisible: true, minimumWidth: 82, topMargin: 0.16, bottomMargin: 0.12 },
            timeScale: { barSpacing: 7, rightOffset: 5, minBarSpacing: 2, maxBarSpacing: 24, minimumHeight: 30 },
            crosshair: {
                mode: "magnet",
                vertLine: { color: "#2f6f8f", style: 2, width: 1, labelVisible: true, labelBackgroundColor: "#2f6f8f" },
                horzLine: { color: "#7b6fc5", style: 4, width: 1, labelVisible: true, labelBackgroundColor: "#7b6fc5" }
            }
        })
        strategyChart.setPaneStretchFactor(0, 3)
        strategyChart.setPaneStretchFactor(1, 1)
        strategyChart.setVisiblePriceRange(0, 91.5, 108.5)

        sessionChart.applyOptions({
            handleScroll: { enabled: true, mouseWheel: true, pressedMouseMove: true },
            handleScale: { enabled: true, mouseWheel: true, pinch: true },
            rightPriceScale: { borderVisible: true, tickMarksVisible: true, minimumWidth: 86, tickSpacing: 44 },
            timeScale: { barSpacing: 8.5, rightOffset: 4, minBarSpacing: 2, maxBarSpacing: 32, fixLeftEdge: true, rightBarStaysOnScroll: true, minimumHeight: 34, maxLabelWidth: 84 },
            localization: { localeName: "en_US", pricePrecision: 0, priceFormat: "volume", timeVisible: true, secondsVisible: false }
        })
        sessionChart.setPaneHeight(1, 84)
        sessionChart.moveSeriesInPane(sessionLine, 1)

        var extraPane = apiChart.addPane()
        temporaryPaneLine.pane = extraPane
        temporaryPaneLine.setData([
            { time: "2024-01-03", value: 430000 },
            { time: { year: 2024, month: 1, day: 4 }, value: 445000 },
            { time: Date.UTC(2024, 0, 5) / 1000, value: 438000 }
        ])
        apiChart.swapPanes(1, extraPane)
        apiChart.removeSeries(temporaryPaneLine)
        apiChart.removePane(1)
    }

    function loadData() {
        priceCandles.setData(root.marketCandles)
        emaLine.setData(root.marketAverage)
        overlayVolumeBars.setData(root.marketOverlayVolume)
        volumeBars.setData(root.marketVolume)
        ohlcBars.setData(root.ohlcRows)
        rangeArea.setData(root.areaRows)
        returnBaseline.setData(root.baselineRows)
        breadthBars.setData(root.breadthRows)
        flowLine.setData(root.apiLineRows)
        flowBars.setData(root.apiHistogramRows)
        strategyLine.setData(root.strategyRows)
        strategyBand.setData(root.strategyBandRows)
        strategyExposure.setData(root.strategyExposureRows)
        sessionLine.setData(root.sessionRows)
        sessionAverage.setData(root.sessionAverageRows)
        sessionEvents.setData(root.sessionEventRows)

        priceCandles.setMarkers([
            { id: "buy-18", time: root.marketCandles[18].time, position: "belowBar", shape: "arrowUp", color: "#1fa187", size: 1.1, text: "B" },
            { id: "sell-54", time: root.marketCandles[54].time, position: "aboveBar", shape: "arrowDown", color: "#d95262", size: 1.1, text: "S" },
            { id: "note-91", time: root.marketCandles[91].time, position: "inBar", shape: "circle", color: "#d7a72b", size: 0.9 },
            { id: "top-118", time: root.marketCandles[118].time, position: "atPriceTop", shape: "square", price: root.marketCandles[118].high, color: "#2d7dd2", size: 0.8 },
            { id: "mid-126", time: root.marketCandles[126].time, position: "atPrice", shape: "circle", price: root.marketCandles[126].close, color: "#7b6fc5", size: 0.8 },
            { id: "low-135", time: root.marketCandles[135].time, position: "atPriceBottom", shape: "square", price: root.marketCandles[135].low, color: "#c4455c", size: 0.8 }
        ])
        returnBaseline.setMarkers([
            { id: "zero-32", time: root.baselineRows[32].time, position: "atPrice", shape: "circle", price: 0, color: "#49566a", size: 0.7 },
            { id: "run-103", time: root.baselineRows[103].time, position: "aboveBar", shape: "square", color: "#248b72", size: 0.8 }
        ])
        flowLine.setMarkers([
            { id: "flow-22", time: root.apiLineRows[22].time, position: "belowBar", shape: "arrowUp", color: "#2a9d8f", size: 0.9 },
            { id: "flow-63", time: root.apiLineRows[63].time, position: "aboveBar", shape: "arrowDown", color: "#c4455c", size: 0.9 }
        ])
        strategyLine.setMarkers([
            { id: "risk-21", time: root.strategyRows[21].time, position: "atPrice", shape: "square", price: 96.0, color: "#c4455c", size: 0.8 },
            { id: "rebalance-68", time: root.strategyRows[68].time, position: "belowBar", shape: "arrowUp", color: "#2f6f8f", size: 0.9, text: "R" },
            { id: "target-109", time: root.strategyRows[109].time, position: "atPriceTop", shape: "circle", price: 106.4, color: "#7b6fc5", size: 0.8 }
        ])
        sessionLine.setMarkers([
            { id: "open-5", time: root.sessionRows[5].time, position: "belowBar", shape: "circle", color: "#3b6ea8", size: 0.8 },
            { id: "gap-47", time: root.sessionRows[47].time, position: "aboveBar", shape: "square", color: "#c4455c", size: 0.9, text: "G" },
            { id: "close-72", time: root.sessionRows[72].time, position: "aboveBar", shape: "arrowDown", color: "#b36b42", size: 0.9 }
        ])

        strategyLine.createPriceLine({
            id: "floor",
            price: 96.0,
            color: "#c4455c",
            lineWidth: 1.2,
            lineStyle: 2,
            title: "Floor",
            axisLabelTextColor: "#ffffff",
            axisLabelBackgroundColor: "#c4455c"
        })
        strategyLine.createPriceLine({
            id: "target",
            price: 106.4,
            color: "#7b6fc5",
            lineWidth: 1.6,
            lineStyle: 0,
            title: "Target",
            axisLabelTextColor: "#ffffff",
            axisLabelBackgroundColor: "#7b6fc5"
        })
        strategyLine.createPriceLine({
            id: "mid",
            price: 100.0,
            color: "#49566a",
            lineWidth: 1,
            lineStyle: 4,
            title: "Base",
            axisLabelVisible: false
        })

        flowLine.update({ time: root.apiLineRows[20].time, value: 525000 }, true)
        root.apiLineRows[20] = { time: root.apiLineRows[20].time, value: 525000 }
        flowLine.pop(1)
        flowBars.pop(1)
        root.apiLineRows.pop()
        root.apiHistogramRows.pop()
    }

    function fitAll() {
        marketChart.fitContent()
        ohlcChart.fitContent()
        percentChart.fitContent()
        apiChart.fitContent()
        strategyChart.fitContent()
        sessionChart.fitContent()
        root.statusText = "Fit content"
        root.refreshStats()
    }

    function zoomWindow() {
        if (root.marketCandles.length < 120 || root.apiLineRows.length < 70 || root.strategyRows.length < 110 || root.sessionRows.length < 70) {
            return
        }
        marketChart.setVisibleTimeRange(root.marketCandles[36].time, root.marketCandles[112].time)
        ohlcChart.setVisibleLogicalRange(28, 98)
        percentChart.setVisibleTimeRange(root.baselineRows[24].time, root.baselineRows[126].time)
        apiChart.setVisibleLogicalRange(10, 68)
        strategyChart.setVisibleLogicalRange(18, 106)
        sessionChart.setVisibleLogicalRange(8, 70)
        root.statusText = "Windowed ranges"
        root.refreshStats()
    }

    function scrollAllToRealtime() {
        marketChart.scrollToRealTime()
        ohlcChart.scrollToRealTime()
        percentChart.scrollToRealTime()
        apiChart.scrollToPosition(0)
        strategyChart.scrollToRealTime()
        sessionChart.scrollToRealTime()
        root.statusText = "Right offset " + formatNumber(marketChart.scrollPosition(), 1)
        root.refreshStats()
    }

    function togglePinnedCrosshairs() {
        if (root.pinnedCrosshair) {
            marketChart.clearCrosshairPosition()
            ohlcChart.clearCrosshairPosition()
            percentChart.clearCrosshairPosition()
            apiChart.clearCrosshairPosition()
            strategyChart.clearCrosshairPosition()
            sessionChart.clearCrosshairPosition()
            root.pinnedCrosshair = false
            root.statusText = "Crosshair cleared"
            return
        }

        if (root.marketCandles.length === 0 || root.apiLineRows.length === 0 || root.strategyRows.length === 0 || root.sessionRows.length === 0) {
            return
        }
        var index = Math.min(root.focusIndex, root.marketCandles.length - 1)
        var candle = root.marketCandles[index]
        var percent = root.baselineRows[index]
        var apiIndex = Math.min(36, root.apiLineRows.length - 1)
        var flow = root.apiLineRows[apiIndex]
        var flowPrice = flow.value !== undefined ? flow.value : 460000
        var strategyIndex = Math.min(index, root.strategyRows.length - 1)
        var strategy = root.strategyRows[strategyIndex]
        var session = nearestRowWithValue(root.sessionRows, Math.min(42, root.sessionRows.length - 1), 320000)
        marketChart.setCrosshairPosition(candle.time, candle.close, 0)
        ohlcChart.setCrosshairPosition(candle.time, candle.close, 0)
        percentChart.setCrosshairPosition(percent.time, percent.value, 0)
        apiChart.setCrosshairPosition(flow.time, flowPrice, 0)
        strategyChart.setCrosshairPosition(strategy.time, strategy.value, 0)
        sessionChart.setCrosshairPosition(session.time, session.value, 0)
        root.pinnedCrosshair = true
        root.statusText = "Pinned " + formatDate(candle.time)
        root.refreshStats()
    }

    function appendLivePoint() {
        if (root.marketCandles.length === 0 || root.apiLineRows.length === 0) {
            return
        }

        var last = root.marketCandles[root.marketCandles.length - 1]
        var i = root.marketCandles.length + root.liveTick
        var time = last.time + 24 * 60 * 60
        var open = last.close
        var close = open + Math.sin(i / 3.0) * 0.9 + Math.cos(i / 11.0) * 0.55
        var high = Math.max(open, close) + 0.95 + (i % 5) * 0.14
        var low = Math.min(open, close) - 0.8 - (i % 6) * 0.12
        var candle = { time: time, open: open, high: high, low: low, close: close }
        root.marketCandles.push(candle)
        root.ohlcRows.push(candle)
        priceCandles.update(candle)
        ohlcBars.update(candle)

        var ema = root.marketAverage[root.marketAverage.length - 1].value * 0.84 + close * 0.16
        var average = { time: time, value: ema }
        root.marketAverage.push(average)
        emaLine.update(average)

        var area = { time: time, value: low + (high - low) * 0.42 }
        root.areaRows.push(area)
        rangeArea.update(area)

        var volumeValue = 820000 + Math.abs(close - open) * 260000 + (i % 10) * 42000
        var volume = { time: time, value: volumeValue, color: close >= open ? "#2a9d8f" : "#c4455c" }
        var overlayVolume = { time: time, value: volumeValue, color: close >= open ? "#442a9d8f" : "#44c4455c" }
        root.marketVolume.push(volume)
        root.marketOverlayVolume.push(overlayVolume)
        overlayVolumeBars.update(overlayVolume)
        volumeBars.update(volume)

        var percent = { time: time, value: (close - 128.0) / 128.0 * 100.0 }
        root.baselineRows.push(percent)
        returnBaseline.update(percent)

        var breadthValue = 52 + Math.sin(i / 8.0) * 20 + Math.cos(i / 17.0) * 12
        var breadth = { time: time, value: breadthValue, color: breadthValue >= 50 ? "#339b7d" : "#c44b5a" }
        root.breadthRows.push(breadth)
        breadthBars.update(breadth)

        var lastFlowTime = root.apiLineRows[root.apiLineRows.length - 1].time
        var j = root.apiLineRows.length + root.liveTick
        var impulse = Math.sin(j / 4.0) * 46000 + Math.cos(j / 10.0) * 26000 + j * 700
        var flow = { time: lastFlowTime + 15 * 60, value: 460000 + impulse }
        var flowBar = { time: flow.time, value: 120000 + Math.abs(impulse) * 1.8 + (j % 9) * 9000, color: impulse >= 0 ? "#2a9d8f" : "#c4455c" }
        root.apiLineRows.push(flow)
        root.apiHistogramRows.push(flowBar)
        flowLine.update(flow)
        flowBars.update(flowBar)

        if (root.strategyRows.length > 0) {
            var lastStrategy = root.strategyRows[root.strategyRows.length - 1]
            var s = root.strategyRows.length + root.liveTick
            var strategyTime = lastStrategy.time + 24 * 60 * 60
            var strategyValue = lastStrategy.value + Math.sin(s / 4.5) * 0.42 + Math.cos(s / 13.0) * 0.28
            var bandValue = strategyValue - 2.6 - Math.abs(Math.sin(s / 8.0)) * 2.4
            var exposureValue = 42 + Math.sin(s / 6.5) * 24 + Math.cos(s / 20.0) * 14
            var strategyPoint = { time: strategyTime, value: strategyValue, color: strategyValue >= 100 ? "#2f6f8f" : "#c4455c" }
            var bandPoint = {
                time: strategyTime,
                value: bandValue,
                lineColor: bandValue < 96 ? "#c4455c" : "#4f8f72",
                topColor: bandValue < 96 ? "#c4455c33" : "#4f8f7244",
                bottomColor: bandValue < 96 ? "#c4455c08" : "#4f8f7208"
            }
            var exposurePoint = { time: strategyTime, value: exposureValue, color: exposureValue >= 50 ? "#6e8f5b" : "#c28b3c" }
            root.strategyRows.push(strategyPoint)
            root.strategyBandRows.push(bandPoint)
            root.strategyExposureRows.push(exposurePoint)
            strategyLine.update(strategyPoint)
            strategyBand.update(bandPoint)
            strategyExposure.update(exposurePoint)
        }

        if (root.sessionRows.length > 0) {
            var lastSessionTime = root.sessionRows[root.sessionRows.length - 1].time
            var q = root.sessionRows.length + root.liveTick
            var sessionImpulse = Math.sin(q / 4.5) * 68000 + Math.cos(q / 14.0) * 48000
            var liquidity = 320000 + sessionImpulse + q * 1100
            var sessionPoint = { time: lastSessionTime + 10 * 60, value: liquidity, color: liquidity >= 320000 ? "#3b6ea8" : "#b36b42" }
            var previousAverage = root.sessionAverageRows[root.sessionAverageRows.length - 1].value
            var averagePoint = { time: sessionPoint.time, value: previousAverage * 0.9 + liquidity * 0.1 }
            var eventPoint = { time: sessionPoint.time, value: Math.abs(sessionImpulse) * 0.45 + (q % 7) * 7500, color: "#6e8f5b" }
            root.sessionRows.push(sessionPoint)
            root.sessionAverageRows.push(averagePoint)
            root.sessionEventRows.push(eventPoint)
            sessionLine.update(sessionPoint)
            sessionAverage.update(averagePoint)
            sessionEvents.update(eventPoint)
        }

        root.focusIndex = Math.min(root.marketCandles.length - 1, root.focusIndex + 1)
        root.liveTick += 1
        root.statusText = "Update " + formatDate(time) + " close " + formatNumber(close, 2)
        root.refreshStats()
    }

    function refreshStats() {
        if (root.marketCandles.length === 0) {
            return
        }

        marketChart.resizeChart(marketChart.chartWidth(), marketChart.chartHeight())

        var range = marketChart.visibleLogicalRange()
        if (range && range.from !== undefined) {
            root.rangeText = "Logical " + formatNumber(range.from, 1) + " - " + formatNumber(range.to, 1)
        }
        var timeRange = marketChart.visibleTimeRange()
        if (timeRange && timeRange.from !== undefined) {
            root.rangeText += " | " + formatDate(timeRange.from) + " - " + formatDate(timeRange.to)
        }

        var from = range && range.from !== undefined ? Math.max(0, Math.floor(range.from)) : 0
        var to = range && range.to !== undefined ? Math.min(root.marketCandles.length - 1, Math.ceil(range.to)) : root.marketCandles.length - 1
        var bars = emaLine.barsInLogicalRange(from, to)
        var index = Math.max(0, Math.min(root.marketCandles.length - 1, Math.round(root.focusIndex)))
        var row = priceCandles.dataByIndex(index, 0)
        var flowFallback = flowLine.dataByIndex(20, -1)
        var flowRows = flowLine.data().length
        var closeText = row.close !== undefined ? formatNumber(row.close, 2) : "-"
        var flowText = flowFallback.value !== undefined ? formatNumber(flowFallback.value, 0) : "-"
        var beforeAfter = bars && bars.barsBefore !== undefined ? bars.barsBefore + "/" + bars.barsAfter : "-/-"
        root.dataText = "EMA " + beforeAfter + " | close " + closeText + " | flow " + flowRows + " " + flowText

        if (root.strategyRows.length > 0) {
            var priceRange = strategyChart.visiblePriceRange(0)
            var priceLines = strategyLine.priceLines()
            var strategyOrder = strategyChart.seriesOrderInPane(strategyLine)
            var strategyPane = strategyChart.paneSize(1)
            var rangeLabel = priceRange && priceRange.min !== undefined
                    ? formatNumber(priceRange.min, 1) + "-" + formatNumber(priceRange.max, 1)
                    : "-"
            root.layoutText = priceLines.length + " lines"
                    + " | range " + rangeLabel
                    + " | order " + strategyOrder
                    + " | exposure " + Math.round(strategyPane.height || 0)
        }

        if (root.sessionRows.length > 0) {
            var sessionRange = sessionChart.visibleLogicalRange()
            var sessionFrom = sessionRange && sessionRange.from !== undefined ? Math.max(0, Math.floor(sessionRange.from)) : 0
            var sessionTo = sessionRange && sessionRange.to !== undefined ? Math.min(root.sessionRows.length - 1, Math.ceil(sessionRange.to)) : root.sessionRows.length - 1
            var sessionBars = sessionLine.barsInLogicalRange(sessionFrom, sessionTo)
            var eventPane = sessionChart.paneSize(1)
            var order = sessionChart.seriesOrderInPane(sessionLine)
            var sessionBeforeAfter = sessionBars && sessionBars.barsBefore !== undefined ? sessionBars.barsBefore + "/" + sessionBars.barsAfter : "-/-"
            root.formatterText = "Rows " + sessionLine.data().length
                    + " | gaps " + root.sessionGapCount
                    + " | " + sessionBeforeAfter
                    + " | order " + order
                    + " | events " + Math.round(eventPane.height || 0)
        }

        var x = marketChart.timeToCoordinate(row.time)
        var y = marketChart.priceToCoordinate(0, row.close)
        var payload = isFinite(x) && isFinite(y) ? marketChart.coordinateToData(Qt.point(x, y)) : ({})
        var logical = isFinite(x) ? marketChart.coordinateToLogical(x) : 0
        var time = isFinite(x) ? marketChart.coordinateToTime(x) : 0
        var roundTripIndex = marketChart.timeToIndex(time)
        var roundTripTime = marketChart.indexToTime(roundTripIndex)
        var pane = marketChart.paneSize(0)
        var panes = marketChart.paneSizes()
        var opts = marketChart.options()
        var spacing = opts.timeScale ? opts.timeScale.barSpacing : marketChart.barSpacing
        var priceLabel = payload.priceLabel || formatNumber(row.close, 2)
        root.geometryText = Math.round(marketChart.chartWidth()) + "x" + Math.round(marketChart.chartHeight())
                + " | pane " + Math.round(pane.width || 0) + "x" + Math.round(pane.height || 0)
                + " | " + panes.length + " panes"
                + " | L" + formatNumber(logical, 1)
                + " I" + roundTripIndex
                + " T" + (roundTripTime ? formatDate(roundTripTime) : "-")
                + " S" + formatNumber(spacing, 1)
                + " P" + priceLabel
    }
}
