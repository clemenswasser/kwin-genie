// SPDX-FileCopyrightText: 2026 Clemens Wasser
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <KCModule>
#include <KPluginMetaData>

#include "ui_genie_config.h"

class GenieEffectConfig : public KCModule
{
    Q_OBJECT

public:
    explicit GenieEffectConfig(QObject *parent, const KPluginMetaData &data);
    ~GenieEffectConfig() override;

    void save() override;

private:
    Ui::GenieConfigUi ui;
};
