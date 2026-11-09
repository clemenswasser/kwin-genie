
kcoreaddons_add_plugin(genie
    SOURCES
        main.cpp
        genieeffect.cpp
        genie.c
    INSTALL_NAMESPACE "kwin/effects/plugins"
)

target_link_libraries(genie PRIVATE
    KWin::kwin
    KF6::CoreAddons
)

# genie.h mirrors the Dock's float/double arithmetic; keep the compiler from contracting it into FMAs.
set_source_files_properties(genie.c PROPERTIES COMPILE_OPTIONS "-ffp-contract=off")
