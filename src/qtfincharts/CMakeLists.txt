include(GenerateExportHeader)

set(qtfincharts_public_headers
    QtFinCharts.h
    core/ChartModel.h
    core/PriceScale.h
    core/SeriesModel.h
    core/TimeScale.h
    core/Types.h
    quick/AreaSeries.h
    quick/BarSeries.h
    quick/BaselineSeries.h
    quick/CandlestickSeries.h
    quick/ChartSeries.h
    quick/ChartTheme.h
    quick/ColorParsing.h
    quick/FinancialChartItem.h
    quick/HistogramSeries.h
    quick/LineSeries.h
)

set(qtfincharts_sources
    core/ChartModel.cpp
    core/PriceScale.cpp
    core/SeriesModel.cpp
    core/TimeScale.cpp
    quick/AreaSeries.cpp
    quick/BarSeries.cpp
    quick/BaselineSeries.cpp
    quick/CandlestickSeries.cpp
    quick/ChartSeries.cpp
    quick/ChartTheme.cpp
    quick/ColorParsing.cpp
    quick/FinancialChartItem.cpp
    quick/HistogramSeries.cpp
    quick/LineSeries.cpp
)

qt_add_library(QtFinCharts ${QTFINCHARTS_LIBRARY_TYPE})
add_library(QtFinCharts::QtFinCharts ALIAS QtFinCharts)

generate_export_header(QtFinCharts
    BASE_NAME QTFINCHARTS
    EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/qtfincharts_export.h
)

qt_add_qml_module(QtFinCharts
    URI ${QTFINCHARTS_QML_IMPORT_URI}
    VERSION ${QTFINCHARTS_QML_IMPORT_VERSION}
    RESOURCE_PREFIX /qt/qml
    OUTPUT_DIRECTORY
        ${QTFINCHARTS_QML_OUTPUT_BASE_DIR}/QtFinCharts
    SOURCES
        ${qtfincharts_sources}
        ${qtfincharts_public_headers}
)

target_sources(QtFinCharts
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS
            ${CMAKE_CURRENT_SOURCE_DIR}
            ${CMAKE_CURRENT_BINARY_DIR}
        FILES
            ${qtfincharts_public_headers}
            ${CMAKE_CURRENT_BINARY_DIR}/qtfincharts_export.h
)

target_include_directories(QtFinCharts
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/core>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/quick>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/QtFinCharts>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/QtFinCharts/core>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/QtFinCharts/quick>
)

target_compile_features(QtFinCharts
    PUBLIC
        cxx_std_20
)

target_link_libraries(QtFinCharts
    PUBLIC
        Qt6::Core
        Qt6::Gui
        Qt6::Qml
        Qt6::Quick
)

target_compile_definitions(QtFinCharts
    PRIVATE
        QT_NO_KEYWORDS
        QT_DISABLE_DEPRECATED_BEFORE=0x060500
)

get_target_property(qtfincharts_target_type QtFinCharts TYPE)
if(qtfincharts_target_type STREQUAL "STATIC_LIBRARY")
    target_compile_definitions(QtFinCharts
        PUBLIC
            QTFINCHARTS_STATIC_DEFINE
    )
endif()
