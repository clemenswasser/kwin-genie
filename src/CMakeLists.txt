# SPDX-FileCopyrightText: 2026 Clemens Wasser
# SPDX-License-Identifier: GPL-2.0-or-later

set(genie_SRCS
    main.cpp
    genieeffect.cpp
    genie.c
)

kconfig_add_kcfg_files(genie_SRCS genieconfig.kcfgc)

kcoreaddons_add_plugin(genie
    SOURCES
        ${genie_SRCS}
    INSTALL_NAMESPACE "kwin/effects/plugins"
)

target_link_libraries(genie PRIVATE
    KWin::kwin
    KF6::CoreAddons
    KF6::ConfigCore
    KF6::ConfigGui
)

add_subdirectory(kcm)

# genie.h mirrors the Dock's float/double arithmetic; keep the compiler from contracting it into FMAs.
set_source_files_properties(genie.c PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
