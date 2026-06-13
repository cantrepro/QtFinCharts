qt_add_executable(QtFinChartsShowcase
    main.cpp
)

qt_add_qml_module(QtFinChartsShowcase
    URI QtFinCharts.Showcase
    VERSION 1.0
    IMPORT_PATH
        ${QTFINCHARTS_QML_OUTPUT_BASE_DIR}
    QML_FILES
        Main.qml
)

target_link_libraries(QtFinChartsShowcase
    PRIVATE
        Qt6::Quick
        QtFinCharts::QtFinCharts
)
