cmake_minimum_required(VERSION 3.24)

project(QtFinCharts
    VERSION 0.1.0
    DESCRIPTION "Native Qt Quick financial chart components"
    LANGUAGES CXX
)

include(GNUInstallDirs)

set(QTFINCHARTS_QT_MIN_VERSION 6.5 CACHE STRING "Minimum Qt 6 version required by QtFinCharts")
set(QTFINCHARTS_QML_IMPORT_URI QtFinCharts)
set(QTFINCHARTS_QML_IMPORT_VERSION 1.0)
set(QTFINCHARTS_QML_OUTPUT_BASE_DIR "${CMAKE_CURRENT_BINARY_DIR}/qml")

set(QTFINCHARTS_IS_TOP_LEVEL OFF)
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(QTFINCHARTS_IS_TOP_LEVEL ON)
endif()

option(QTFINCHARTS_BUILD_EXAMPLES "Build QtFinCharts examples" ${QTFINCHARTS_IS_TOP_LEVEL})
option(QTFINCHARTS_INSTALL "Generate QtFinCharts install and CMake package rules" ${QTFINCHARTS_IS_TOP_LEVEL})

set(QTFINCHARTS_LIBRARY_TYPE "" CACHE STRING "Library type to build: empty, STATIC, or SHARED")
set_property(CACHE QTFINCHARTS_LIBRARY_TYPE PROPERTY STRINGS "" STATIC SHARED)

set(QTFINCHARTS_INSTALL_QMLDIR "${CMAKE_INSTALL_LIBDIR}/qml" CACHE PATH "Install directory for QtFinCharts QML module files")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 ${QTFINCHARTS_QT_MIN_VERSION} REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES ${QTFINCHARTS_QT_MIN_VERSION})

add_subdirectory(src/qtfincharts)

if(QTFINCHARTS_BUILD_EXAMPLES)
    add_subdirectory(examples/basic)
    add_subdirectory(examples/showcase)
endif()

if(QTFINCHARTS_INSTALL)
    include(CMakePackageConfigHelpers)

    install(TARGETS QtFinCharts
        EXPORT QtFinChartsTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        FILE_SET HEADERS DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/QtFinCharts
        INCLUDES DESTINATION
            ${CMAKE_INSTALL_INCLUDEDIR}
            ${CMAKE_INSTALL_INCLUDEDIR}/QtFinCharts
    )

    if(TARGET QtFinChartsplugin)
        install(TARGETS QtFinChartsplugin
            EXPORT QtFinChartsTargets
            RUNTIME DESTINATION ${QTFINCHARTS_INSTALL_QMLDIR}/QtFinCharts
            LIBRARY DESTINATION ${QTFINCHARTS_INSTALL_QMLDIR}/QtFinCharts
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        )
    endif()

    if(TARGET QtFinChartsplugin_init)
        install(TARGETS QtFinChartsplugin_init
            EXPORT QtFinChartsTargets
        )
    endif()

    if(TARGET QtFinCharts_resources_1)
        install(TARGETS QtFinCharts_resources_1
            EXPORT QtFinChartsTargets
            OBJECTS DESTINATION ${CMAKE_INSTALL_LIBDIR}/QtFinCharts/objects
        )
    endif()

    install(DIRECTORY "${QTFINCHARTS_QML_OUTPUT_BASE_DIR}/QtFinCharts/"
        DESTINATION ${QTFINCHARTS_INSTALL_QMLDIR}/QtFinCharts
        OPTIONAL
        FILES_MATCHING
            PATTERN "qmldir"
            PATTERN "*.qmltypes"
            PATTERN "*.metatypes"
    )

    set(qtfincharts_config_install_dir "${CMAKE_INSTALL_LIBDIR}/cmake/QtFinCharts")
    configure_package_config_file(
        "${PROJECT_SOURCE_DIR}/cmake/QtFinChartsConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/QtFinChartsConfig.cmake"
        INSTALL_DESTINATION ${qtfincharts_config_install_dir}
    )
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/QtFinChartsConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(EXPORT QtFinChartsTargets
        NAMESPACE QtFinCharts::
        FILE QtFinChartsTargets.cmake
        DESTINATION ${qtfincharts_config_install_dir}
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/QtFinChartsConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/QtFinChartsConfigVersion.cmake"
        DESTINATION ${qtfincharts_config_install_dir}
    )

    install(FILES
        README.md
        LICENSE
        DESTINATION ${CMAKE_INSTALL_DOCDIR}
    )
endif()
