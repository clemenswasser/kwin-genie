// SPDX-FileCopyrightText: 2026 Clemens Wasser
// SPDX-License-Identifier: GPL-2.0-or-later
#include "genieeffect.h"

namespace KWin
{

KWIN_EFFECT_FACTORY_SUPPORTED(GenieEffect, "metadata.json", return GenieEffect::supported();)

}

#include "main.moc"
