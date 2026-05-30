cmake_minimum_required(VERSION 3.21)
project(yandex-messenger-qt VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

include(GNUInstallDirs)

find_package(Qt6 6.4 REQUIRED COMPONENTS Widgets WebEngineWidgets)

add_executable(yandex-messenger-qt
    src/main.cpp
)

target_link_libraries(yandex-messenger-qt PRIVATE
    Qt6::Widgets
    Qt6::WebEngineWidgets
)

install(TARGETS yandex-messenger-qt
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES assets/yandex-messenger-qt.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

install(FILES packaging/yandex-messenger-qt.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

set(CPACK_PACKAGE_NAME "yandex-messenger-qt")
set(CPACK_PACKAGE_VENDOR "glebrodionov94")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "Unofficial Qt 6 wrapper for Yandex Messenger")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_CONTACT "glebrodionov94")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "glebrodionov94")
set(CPACK_DEBIAN_PACKAGE_SECTION "net")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS
    "xdg-desktop-portal, xdg-desktop-portal-kde, pipewire")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

include(CPack)
