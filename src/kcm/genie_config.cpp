// SPDX-FileCopyrightText: 2026 Clemens Wasser
// SPDX-License-Identifier: GPL-2.0-or-later

#include "genie_config.h"
#include "genieconfig.h"

#include <QDBusConnection>
#include <kwineffects_interface.h>

GenieEffectConfig::GenieEffectConfig(QObject *parent, const KPluginMetaData &data)
    : KCModule(parent, data)
{
    ui.setupUi(widget());
    addConfig(KWin::GenieConfig::self(), widget());
}

GenieEffectConfig::~GenieEffectConfig() = default;

void GenieEffectConfig::save()
{
    KCModule::save();
    OrgKdeKwinEffectsInterface interface(QStringLiteral("org.kde.KWin"),
                                         QStringLiteral("/Effects"),
                                         QDBusConnection::sessionBus());
    interface.reconfigureEffect(QStringLiteral("genie"));
}
