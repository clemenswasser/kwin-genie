#pragma once

#include "effect/effectwindow.h"
#include "effect/offscreeneffect.h"
#include "effect/timeline.h"

#include "genie.h"

#include <QHash>
#include <memory>
#include <vector>

namespace KWin
{

class GLShader;

struct GenieAnimation {
    EffectWindowVisibleRef visibleRef;
    TimeLine timeLine;
    int heldFrames = 0;
};

class GenieEffect : public OffscreenEffect
{
    Q_OBJECT

public:
    GenieEffect();
    ~GenieEffect() override;

    void reconfigure(ReconfigureFlags flags) override;
    void prePaintScreen(ScreenPrePaintData &data) override;
    void prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data) override;
    void postPaintScreen() override;
    bool paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen) override;
    bool isActive() const override;

    int requestedEffectChainPosition() const override
    {
        return 50;
    }

    static bool supported();

    struct Target {
        genie_box tile;
        int direction;
    };
    static Target computeTarget(const RectF &frame, const RectF &iconGeometry, const RectF &panelGeometry, const RectF &screen);

protected:
    void apply(EffectWindow *window, int mask, WindowPaintData &data, WindowQuadList &quads) override;

private:
    void slotWindowAdded(EffectWindow *w);
    void slotWindowDeleted(EffectWindow *w);
    void startAnimation(EffectWindow *w, bool minimizing);
    GLShader *projectiveShader();
    RectF panelGeometryFor(const QPointF &anchor) const;

    std::chrono::milliseconds m_duration;
    QHash<EffectWindow *, GenieAnimation> m_animations;
    std::unique_ptr<genie_mesh> m_mesh;
    std::unique_ptr<genie_shadow_mesh> m_shadow;
    std::unique_ptr<GLShader> m_shader;
    int m_weightsLocation = -1;
    int m_texMinLocation = -1;
    int m_clampLocation = -1;
    std::vector<float> m_clamp;
    int m_texMaxLocation = -1;
    std::vector<float> m_weights;

    double m_debugProgress = -1.0;
    RectF m_debugIcon;
    RectF m_debugPanel;
    QString m_debugDump;
    int m_debugFrames = 0;
    bool m_debugDumped = false;
    bool m_exactQuality = false;
    double m_meshPx = 12.0;
    QByteArray m_debugInfo;
};

}
