import QtQuick
import QtFinCharts

Window {
    id: root
    width: 1100
    height: 680
    visible: true
    title: "QtFinCharts Basic"
    color: "#111318"

    ChartTheme {
        id: darkTheme
        backgroundColor: "#111318"
        textColor: "#d6d8df"
        gridColor: "#242934"
        axisColor: "#3a4050"
        crosshairColor: "#9aa4ba"
        crosshairLabelBackground: "#2962ff"
        paneSeparatorColor: "#20242d"
    }

    FinancialChart {
        id: chart
        anchors.fill: parent
        localeName: "en_US"
        pricePrecision: 2
        timeVisible: false
        paneCount: 2
        theme: darkTheme

        CandlestickSeries {
            id: candles
            name: "Price"
            upColor: "#26a69a"
            downColor: "#ef5350"
            wickUpColor: "#26a69a"
            wickDownColor: "#ef5350"
            borderVisible: false
        }

        LineSeries {
            id: average
            name: "Average"
            color: "#f2c94c"
            lineWidth: 2
            lineStyle: 2
            pointMarkersVisible: true
            pointMarkersRadius: 2.4
        }

        AreaSeries {
            id: rangeArea
            name: "Smoothed range"
            topColor: "#335b8cce"
            bottomColor: "#102962ff"
            lineColor: "#5b8cce"
            lineWidth: 1
            relativeGradient: true
        }

        BaselineSeries {
            id: baseline
            name: "Baseline"
            baseValue: 101.5
            topFillColor1: "#4026a69a"
            topFillColor2: "#0826a69a"
            bottomFillColor1: "#08ef5350"
            bottomFillColor2: "#40ef5350"
            topLineColor: "#26a69a"
            bottomLineColor: "#ef5350"
            lineWidth: 2
            pointMarkersVisible: false
        }

        BarSeries {
            id: ohlcBars
            name: "OHLC"
            upColor: "#26a69a"
            downColor: "#ef5350"
            openVisible: true
            thinBars: true
            visible: false
        }

        HistogramSeries {
            id: volume
            name: "Volume"
            pane: 1
            color: "#5b8cce"
            baseValue: 0
        }

        Component.onCompleted: {
            const candleData = []
            const lineData = []
            const areaData = []
            const baselineData = []
            const volumeData = []
            const start = Date.UTC(2024, 0, 2) / 1000
            let previousClose = 101.5
            for (let i = 0; i < 160; ++i) {
                const wave = Math.sin(i / 7.0) * 2.8 + Math.cos(i / 17.0) * 4.2
                const open = previousClose
                const close = 101.5 + wave + i * 0.035
                const high = Math.max(open, close) + 1.2 + (i % 5) * 0.18
                const low = Math.min(open, close) - 1.0 - (i % 7) * 0.14
                const time = start + i * 24 * 60 * 60
                candleData.push({ time, open, high, low, close })
                lineData.push({ time, value: (open + close + high + low) / 4.0 })
                areaData.push({ time, value: low + (high - low) * 0.35 })
                baselineData.push({ time, value: close })
                volumeData.push({
                    time,
                    value: 18 + Math.abs(close - open) * 22 + (i % 9) * 2,
                    color: close >= open ? "#3f9f94" : "#c95a5f"
                })
                previousClose = close
            }

            candles.setData(candleData)
            average.setData(lineData)
            rangeArea.setData(areaData)
            baseline.setData(baselineData)
            ohlcBars.setData(candleData)
            volume.setData(volumeData)
            candles.setMarkers([
                { id: "buy-24", time: candleData[24].time, position: "belowBar", shape: "arrowUp", color: "#26a69a", size: 1.2, text: "B" },
                { id: "sell-73", time: candleData[73].time, position: "aboveBar", shape: "arrowDown", color: "#ef5350", size: 1.2, text: "S" },
                { id: "note-118", time: candleData[118].time, position: "inBar", shape: "circle", color: "#f2c94c", size: 0.9 }
            ])
            chart.fitContent()
        }
    }
}
