cmake_minimum_required(VERSION 3.21)
project(DeskTicker VERSION 0.2.0 LANGUAGES CXX)

if(WIN32)
    enable_language(RC)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(Qt6 6.2 REQUIRED COMPONENTS Widgets)

set(CMAKE_AUTOMOC ON)

qt_add_executable(DeskTicker
    main.cpp
    ClockWidget.cpp
    ClockWidget.h
)

qt_add_resources(DeskTicker app_icons
    PREFIX "/"
    FILES DeskTicker.ico
)

if(WIN32)
    target_sources(DeskTicker PRIVATE DeskTicker.rc)
endif()

target_link_libraries(DeskTicker PRIVATE Qt6::Widgets)
target_compile_definitions(DeskTicker PRIVATE
    DESKTICKER_VERSION=\"${PROJECT_VERSION}\"
    "$<$<CONFIG:Debug>:DESKTICKER_DEBUG_BUILD>"
)

if(WIN32)
    set_target_properties(DeskTicker PROPERTIES WIN32_EXECUTABLE TRUE)

    get_filename_component(QT_INSTALL_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt
        HINTS "${QT_INSTALL_PREFIX}/bin"
        REQUIRED
    )

    add_custom_command(TARGET DeskTicker POST_BUILD
        COMMAND "${WINDEPLOYQT_EXECUTABLE}"
            "$<IF:$<CONFIG:Debug>,--debug,--release>"
            --compiler-runtime
            --no-translations
            "$<TARGET_FILE:DeskTicker>"
        COMMENT "Deploying Qt runtime for DeskTicker"
        VERBATIM
    )
endif()

if(APPLE)
    set_target_properties(DeskTicker PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "DeskTicker"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.deskticker.app"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
    )

    get_filename_component(QT_INSTALL_PREFIX "${Qt6_DIR}/../../.." ABSOLUTE)
    find_program(MACDEPLOYQT_EXECUTABLE macdeployqt
        HINTS "${QT_INSTALL_PREFIX}/bin"
        REQUIRED
    )

    add_custom_command(TARGET DeskTicker POST_BUILD
        COMMAND "${MACDEPLOYQT_EXECUTABLE}" "$<TARGET_BUNDLE_DIR:DeskTicker>" -always-overwrite
        COMMENT "Deploying Qt runtime into DeskTicker.app"
        VERBATIM
    )
endif()

if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS DeskTicker RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}")
    install(FILES packaging/DeskTicker.desktop
        DESTINATION "${CMAKE_INSTALL_DATAROOTDIR}/applications")

    set(CPACK_GENERATOR "DEB")
    set(CPACK_PACKAGE_DIRECTORY "${CMAKE_BINARY_DIR}")
    set(CPACK_PACKAGE_NAME "DeskTicker")
    set(CPACK_PACKAGE_FILE_NAME "DeskTicker-${PROJECT_VERSION}-Linux")
    set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "A compact desktop clock widget")
    set(CPACK_DEBIAN_PACKAGE_MAINTAINER "AnotherLaughingMan")
    set(CPACK_DEBIAN_PACKAGE_SECTION "utils")
    set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
    include(CPack)
endif()
