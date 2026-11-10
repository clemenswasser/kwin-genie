# SPDX-FileCopyrightText: 2026 Clemens Wasser
# SPDX-License-Identifier: GPL-2.0-or-later

set(kcm_SRCS
    genie_config.cpp
    plugin.cpp
)

kconfig_add_kcfg_files(kcm_SRCS
    ../genieconfig.kcfgc
)

qt6_wrap_ui(kcm_SRCS genie_config.ui)

qt6_add_dbus_interface(kcm_SRCS ${KWIN_EFFECTS_INTERFACE} kwineffects_interface)

add_library(kwin_genie_config MODULE ${kcm_SRCS})

target_link_libraries(kwin_genie_config
    Qt6::Core
    Qt6::DBus
    Qt6::Gui
    Qt6::Widgets
    KF6::ConfigCore
    KF6::ConfigGui
    KF6::ConfigWidgets
    KF6::KCMUtils
)

install(
    TARGETS
        kwin_genie_config

    DESTINATION
        ${KDE_INSTALL_QTPLUGINDIR}/kwin/effects/configs/
)
