#include "genieeffect.h"

#include "core/output.h"
#include "core/pixelgrid.h"
#include "core/rendertarget.h"
#include "effect/effecthandler.h"
#include "input.h"
#include "opengl/glshader.h"
#include "opengl/glshadermanager.h"
#include "opengl/glutils.h"

#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <algorithm>
#include <cmath>
#include <limits>

using namespace std::chrono_literals;

namespace KWin
{

static const char s_projectiveVertexShader[] = R"(#version 140

in vec4 position;
in vec4 texcoord;
out vec2 texcoord0;

uniform mat4 modelViewProjectionMatrix;
uniform vec4 genieWeights[GENIE_MAX_QUADS];
uniform float genieClamp[GENIE_MAX_QUADS];
flat out float genieClampFlag;

const int genieCorner[6] = int[6](0, 3, 1, 1, 3, 2);

void main()
{
    texcoord0 = texcoord.st;
    int quad = min(gl_VertexID / 6, GENIE_MAX_QUADS - 1);
    float w = genieWeights[quad][genieCorner[gl_VertexID % 6]];
    genieClampFlag = genieClamp[quad];
    gl_Position = (modelViewProjectionMatrix * vec4(position.xy, 0.0, 1.0)) * w;
}
)";

static constexpr int s_maxQuads = 1024;

static void caCornerWeights(const float x[4], const float y[4], float w[4])
{
    const float sx = (x[0] + x[2]) - (x[1] + x[3]);
    const float sy = (y[0] + y[2]) - (y[1] + y[3]);
    w[0] = w[1] = w[2] = w[3] = 1.0f;
    if (std::fabs(sx) >= 1e-6f || std::fabs(sy) >= 1e-6f) {
        const float ax = x[1] - x[2], ay = y[1] - y[2];
        const float bx = x[3] - x[2], by = y[3] - y[2];
        const float den = by * ax - bx * ay;
        if (den == 0.0f) {
            return;
        }
        const float inv = 1.0f / den;
        const float g = (ax * sy - ay * sx) * inv;
        const float h1 = (by * sx - bx * sy) * inv + 1.0f;
        w[1] = h1;
        w[2] = h1 + g;
        w[3] = g + 1.0f;
    }
}

GLShader *GenieEffect::projectiveShader()
{
    if (!m_shader) {
        QByteArray source(s_projectiveVertexShader);
        source.replace("GENIE_MAX_QUADS", QByteArray::number(s_maxQuads));

        QByteArray fragment;
        QFile baseFragment(QStringLiteral(":/opengl/base.frag"));
        if (baseFragment.open(QIODevice::ReadOnly)) {
            fragment = baseFragment.readAll();
            fragment.replace("uniform sampler2D sampler;\n",
                             "uniform sampler2D sampler;\nuniform vec2 genieTexMin;\nuniform vec2 genieTexMax;\nflat in float genieClampFlag;\n");
            fragment.replace("result = texture(sampler, texcoord0);",
                             "result = texture(sampler, mix(texcoord0, clamp(texcoord0, genieTexMin, genieTexMax), genieClampFlag));");
        }
        m_shader = ShaderManager::instance()->generateCustomShader(ShaderTrait::MapTexture | ShaderTrait::Modulate | ShaderTrait::AdjustSaturation
                                                                       | ShaderTrait::TransformColorspace,
                                                                   source,
                                                                   fragment);
        if (m_shader) {
            m_weightsLocation = m_shader->uniformLocation("genieWeights");
            m_clampLocation = m_shader->uniformLocation("genieClamp");
            m_texMinLocation = m_shader->uniformLocation("genieTexMin");
            m_texMaxLocation = m_shader->uniformLocation("genieTexMax");
        }
    }
    return m_shader.get();
}

GenieEffect::GenieEffect()
    : m_mesh(std::make_unique<genie_mesh>())
    , m_shadow(std::make_unique<genie_shadow_mesh>())
{
    reconfigure(ReconfigureAll);
    m_weights.reserve(4 * s_maxQuads);
    m_clamp.reserve(s_maxQuads);
    connect(effects, &EffectsHandler::windowAdded, this, &GenieEffect::slotWindowAdded);
    connect(effects, &EffectsHandler::windowDeleted, this, &GenieEffect::slotWindowDeleted);

    const auto windows = effects->stackingOrder();
    for (EffectWindow *window : windows) {
        slotWindowAdded(window);
    }

    setVertexSnappingMode(RenderGeometry::VertexSnappingMode::None);

    bool ok = false;
    const double p = qEnvironmentVariable("GENIE_DEBUG_PROGRESS").toDouble(&ok);
    if (ok) {
        m_debugProgress = std::clamp(p, 0.0, 1.0);
    }
    const QStringList icon = qEnvironmentVariable("GENIE_DEBUG_ICON").split(QLatin1Char(','));
    if (icon.size() == 4) {
        m_debugIcon = RectF(icon[0].toDouble(), icon[1].toDouble(), icon[2].toDouble(), icon[3].toDouble());
    }
    const QStringList panel = qEnvironmentVariable("GENIE_DEBUG_PANEL").split(QLatin1Char(','));
    if (panel.size() == 4) {
        m_debugPanel = RectF(panel[0].toDouble(), panel[1].toDouble(), panel[2].toDouble(), panel[3].toDouble());
    }
    m_debugDump = qEnvironmentVariable("GENIE_DEBUG_DUMP");
    m_exactQuality = qEnvironmentVariable("GENIE_QUALITY") == QLatin1String("exact");
    bool meshPxOk = false;
    const double meshPx = qEnvironmentVariable("GENIE_MESH_PX").toDouble(&meshPxOk);
    if (meshPxOk) {
        m_meshPx = std::clamp(meshPx, 2.0, 64.0);
    }
}

GenieEffect::~GenieEffect() = default;

bool GenieEffect::supported()
{
    return OffscreenEffect::supported() && effects->animationsSupported();
}

void GenieEffect::reconfigure(ReconfigureFlags)
{
    m_duration = animationTime(std::chrono::milliseconds(int(GENIE_DURATION * 1000)));
}

void GenieEffect::prePaintScreen(ScreenPrePaintData &data)
{
    data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen(data);
}

void GenieEffect::prePaintWindow(RenderView *view, EffectWindow *w, WindowPrePaintData &data)
{
    auto it = m_animations.find(w);
    if (it != m_animations.end()) {
        it->timeLine.advance(view);
        data.setTransformed();
    }
    effects->prePaintWindow(view, w, data);
}

bool GenieEffect::paintScreen(const RenderTarget &renderTarget, const RenderViewport &viewport, int mask, const Region &deviceRegion, LogicalOutput *screen)
{
    const bool ok = effects->paintScreen(renderTarget, viewport, mask, deviceRegion, screen);
    if (ok && !m_debugDump.isEmpty() && !m_debugDumped && !m_animations.isEmpty() && ++m_debugFrames >= 5) {
        const QSize size = renderTarget.size();
        QImage image(size, QImage::Format_RGBA8888);
        glReadPixels(0, 0, size.width(), size.height(), GL_RGBA, GL_UNSIGNED_BYTE, image.bits());
        image.save(m_debugDump);
        QFile info(m_debugDump + QStringLiteral(".json"));
        if (info.open(QIODevice::WriteOnly)) {
            info.write(m_debugInfo);
        }
        m_debugDumped = true;
    }
    return ok;
}

void GenieEffect::postPaintScreen()
{
    for (auto it = m_animations.begin(); it != m_animations.end();) {
        if (m_debugProgress < 0 && it->timeLine.done()) {
            if (it->heldFrames++ < 3) {
                ++it;
                continue;
            }
            unredirect(it.key());
            it = m_animations.erase(it);
        } else {
            ++it;
        }
    }
    effects->addRepaintFull();
    effects->postPaintScreen();
}

bool GenieEffect::isActive() const
{
    return !m_animations.isEmpty();
}

void GenieEffect::slotWindowAdded(EffectWindow *w)
{
    connect(w, &EffectWindow::minimizedChanged, this, [this, w]() {
        startAnimation(w, w->isMinimized());
    });
}

void GenieEffect::slotWindowDeleted(EffectWindow *w)
{
    m_animations.remove(w);
}

void GenieEffect::startAnimation(EffectWindow *w, bool minimizing)
{
    if (effects->activeFullScreenEffect()) {
        return;
    }
    GenieAnimation &animation = m_animations[w];
    animation.heldFrames = 0;
    if (animation.timeLine.running()) {
        animation.timeLine.toggleDirection();
    } else {
        animation.visibleRef = EffectWindowVisibleRef(w, EffectWindow::PAINT_DISABLED_BY_MINIMIZE);
        animation.timeLine.setDirection(minimizing ? TimeLine::Forward : TimeLine::Backward);

        const bool slow = input() && (input()->keyboardModifiers() & Qt::ShiftModifier);
        animation.timeLine.setDuration(slow ? m_duration * 10 : m_duration);
        animation.timeLine.setEasingCurve(QEasingCurve::Linear);
    }
    redirect(w);
    if (GLShader *shader = projectiveShader()) {
        setShader(w, shader);
    }
    if (!m_debugDump.isEmpty() && m_debugInfo.isEmpty()) {
        const double scale = w->screen() ? w->screen()->scale() : 1.0;
        const RectF frame = snapToPixels(w->frameGeometry(), scale);
        const RectF iconGeometry = m_debugIcon.isValid() ? m_debugIcon : w->iconGeometry();
        const QPointF anchor = (iconGeometry.isValid() && !iconGeometry.isEmpty()) ? iconGeometry.center() : frame.center();
        const RectF panelGeometry = panelGeometryFor(anchor);
        LogicalOutput *entryScreen = nullptr;
        if (panelGeometry.isValid() && !panelGeometry.isEmpty()) {
            entryScreen = effects->screenAt(panelGeometry.center().toPoint());
        } else if (iconGeometry.isValid() && !iconGeometry.isEmpty()) {
            entryScreen = effects->screenAt(iconGeometry.center().toPoint());
        }
        LogicalOutput *screen = entryScreen ? entryScreen : w->screen();
        const Target t = computeTarget(frame, iconGeometry, panelGeometry, screen->geometryF());
        const genie_box iconRect = genie_icon_rect(frame.width(), frame.height());
        const genie_box target = genie_target_rect(t.tile, iconRect, genie_tile_inset(t.tile.x1 - t.tile.x0));
        const float p = m_debugProgress >= 0 ? float(m_debugProgress) : float(animation.timeLine.value());
        m_debugInfo = QStringLiteral(
                          "{\"p\": %1, \"frame\": [%2, %3, %4, %5], \"target\": [%6, %7, %8, %9], \"dir\": %10, "
                          "\"scale\": %11, \"mesh\": [0, 0], \"icon\": [%12, %13, %14, %15], "
                          "\"panel\": [%16, %17, %18, %19]}")
                          .arg(p)
                          .arg(frame.x())
                          .arg(frame.y())
                          .arg(frame.width())
                          .arg(frame.height())
                          .arg(target.x0)
                          .arg(target.y0)
                          .arg(target.x1)
                          .arg(target.y1)
                          .arg(t.direction)
                          .arg(scale)
                          .arg(iconGeometry.x())
                          .arg(iconGeometry.y())
                          .arg(iconGeometry.width())
                          .arg(iconGeometry.height())
                          .arg(panelGeometry.x())
                          .arg(panelGeometry.y())
                          .arg(panelGeometry.width())
                          .arg(panelGeometry.height())
                          .toUtf8();
    }
    effects->addRepaintFull();
}

static genie_quality smoothQuality(double scale, double mesh_px)
{
    genie_quality q = genie_quality_exact;
    const double s = std::max(scale, 1.0);
    q.min_span = mesh_px / s;   // device pixels
    q.chord_tolerance = 0.0;    // force the max-segment density
    q.chord_max_span = 0.0;
    q.max_visits = 4096;
    q.max_rows = GENIE_MAX_ROWS;
    return q;
}

static double rectPointDistance(const RectF &rect, const QPointF &point)
{
    const double dx = std::max({rect.x() - point.x(), 0.0, point.x() - (rect.x() + rect.width())});
    const double dy = std::max({rect.y() - point.y(), 0.0, point.y() - (rect.y() + rect.height())});
    return std::hypot(dx, dy);
}

RectF GenieEffect::panelGeometryFor(const QPointF &anchor) const
{
    if (m_debugPanel.isValid() && !m_debugPanel.isEmpty()) {
        return m_debugPanel;
    }

    RectF best;
    double bestDistance = std::numeric_limits<double>::max();
    const auto windows = effects->stackingOrder();
    for (EffectWindow *window : windows) {
        if (!window->isDock()) {
            continue;
        }
        const RectF frame = window->frameGeometry();
        if (!frame.isValid() || frame.isEmpty()) {
            continue;
        }
        const double distance = rectPointDistance(frame, anchor);
        if (distance < bestDistance) {
            bestDistance = distance;
            best = frame;
        }
    }
    return best;
}

GenieEffect::Target GenieEffect::computeTarget(const RectF &frame, const RectF &iconGeometry, const RectF &panelGeometry, const RectF &screen)
{
    RectF icon = iconGeometry;
    const bool havePanel = panelGeometry.isValid() && !panelGeometry.isEmpty();
    const bool panelHorizontal = havePanel && panelGeometry.width() >= panelGeometry.height();

    if (!icon.isValid() || icon.isEmpty()) {
        if (havePanel) {
            const double thickness = panelHorizontal ? panelGeometry.height() : panelGeometry.width();
            const double side = std::max(thickness - 24.0, 16.0);
            const QPointF windowCenter = frame.center();
            QPointF center;
            if (panelHorizontal) {
                const double lo = panelGeometry.x() + side / 2 + 6;
                const double hi = panelGeometry.x() + panelGeometry.width() - side / 2 - 6;
                center = QPointF(std::clamp(windowCenter.x(), std::min(lo, hi), std::max(lo, hi)), panelGeometry.y() + panelGeometry.height() / 2);
            } else {
                const double lo = panelGeometry.y() + side / 2 + 6;
                const double hi = panelGeometry.y() + panelGeometry.height() - side / 2 - 6;
                center = QPointF(panelGeometry.x() + panelGeometry.width() / 2, std::clamp(windowCenter.y(), std::min(lo, hi), std::max(lo, hi)));
            }
            icon = RectF(center.x() - side / 2, center.y() - side / 2, side, side);
        } else {
            const double side = 48.0;
            icon = RectF(screen.center().x() - side / 2, screen.y() + screen.height() - side, side, side);
        }
    }

    const QPointF c = icon.center();

    double dLeft;
    double dRight;
    double dTop;
    double dBottom;
    if (havePanel) {
        dLeft = std::fabs(panelGeometry.x() - screen.x());
        dRight = std::fabs((screen.x() + screen.width()) - (panelGeometry.x() + panelGeometry.width()));
        dTop = std::fabs(panelGeometry.y() - screen.y());
        dBottom = std::fabs((screen.y() + screen.height()) - (panelGeometry.y() + panelGeometry.height()));
    } else {
        dLeft = c.x() - screen.x();
        dRight = screen.x() + screen.width() - c.x();
        dTop = c.y() - screen.y();
        dBottom = screen.y() + screen.height() - c.y();
    }
    const double dMin = std::min({dLeft, dRight, dTop, dBottom});
    const QPointF wc = frame.center();

    const bool horizontalEdge = havePanel ? panelHorizontal : (dMin == dBottom || dMin == dTop);

    Target t;
    if (horizontalEdge) {
        if (havePanel) {
            // Match the Dock: use the window edge against the panel edge, not the
            // window centre. A window that extends far below a bottom panel has
            // its centre below the icon even though the icon is still below it.
            const bool panelAtBottom = dBottom <= dTop;
            if (panelAtBottom) {
                t.direction = (frame.y() <= panelGeometry.y()) ? GENIE_DIR_DOWN : GENIE_DIR_UP;
            } else {
                t.direction = (frame.y() + frame.height() >= panelGeometry.y() + panelGeometry.height()) ? GENIE_DIR_UP : GENIE_DIR_DOWN;
            }
        } else {
            t.direction = (dTop < dBottom) ? ((wc.y() >= c.y()) ? GENIE_DIR_UP : GENIE_DIR_DOWN) : ((wc.y() <= c.y()) ? GENIE_DIR_DOWN : GENIE_DIR_UP);
        }
    } else {
        if (havePanel) {
            const bool panelAtLeft = dLeft <= dRight;
            if (panelAtLeft) {
                t.direction = (frame.x() + frame.width() <= panelGeometry.x()) ? GENIE_DIR_RIGHT : GENIE_DIR_LEFT;
            } else {
                t.direction = (frame.x() >= panelGeometry.x() + panelGeometry.width()) ? GENIE_DIR_LEFT : GENIE_DIR_RIGHT;
            }
        } else {
            t.direction = (dLeft <= dRight) ? ((wc.x() >= c.x()) ? GENIE_DIR_LEFT : GENIE_DIR_RIGHT) : ((wc.x() <= c.x()) ? GENIE_DIR_RIGHT : GENIE_DIR_LEFT);
        }
    }

    const bool horizontal = horizontalEdge;
    const double cross = horizontal ? icon.height() : icon.width();
    const double along = horizontal ? icon.width() : icon.height();

    const double side = std::max(cross, 1.0);
    QPointF tileCenter = icon.center();
    if (along > cross + 32.0) {
        if (horizontal) {
            tileCenter.setX(icon.x() + side / 2);
        } else {
            tileCenter.setY(icon.y() + side / 2);
        }
    }
    t.tile = genie_box{float(tileCenter.x() - side / 2), float(tileCenter.y() - side / 2), float(tileCenter.x() + side / 2), float(tileCenter.y() + side / 2)};
    return t;
}

void GenieEffect::apply(EffectWindow *w, int mask, WindowPaintData &data, WindowQuadList &quads)
{
    Q_UNUSED(mask)
    Q_UNUSED(data)
    const auto it = m_animations.constFind(w);
    if (it == m_animations.constEnd() || quads.isEmpty()) {
        return;
    }
    const float p = m_debugProgress >= 0 ? float(m_debugProgress) : float(it->timeLine.value());

    const double scale = w->screen() ? w->screen()->scale() : 1.0;
    const RectF frame = snapToPixels(w->frameGeometry(), scale);
    const RectF iconGeometry = m_debugIcon.isValid() ? m_debugIcon : w->iconGeometry();
    const QPointF anchor = (iconGeometry.isValid() && !iconGeometry.isEmpty()) ? iconGeometry.center() : frame.center();
    const RectF panelGeometry = panelGeometryFor(anchor);
    LogicalOutput *entryScreen = nullptr;
    if (panelGeometry.isValid() && !panelGeometry.isEmpty()) {
        entryScreen = effects->screenAt(panelGeometry.center().toPoint());
    } else if (iconGeometry.isValid() && !iconGeometry.isEmpty()) {
        entryScreen = effects->screenAt(iconGeometry.center().toPoint());
    }
    LogicalOutput *screen = entryScreen ? entryScreen : w->screen();
    const Target t = computeTarget(frame, iconGeometry, panelGeometry, screen->geometryF());

    const genie_box iconRect = genie_icon_rect(frame.width(), frame.height());
    const float tileSize = t.tile.x1 - t.tile.x0;
    const genie_box target = genie_target_rect(t.tile, iconRect, genie_tile_inset(tileSize));

    const genie_rect win{frame.x(), frame.y(), frame.width(), frame.height()};
    const genie_quality quality = m_exactQuality ? genie_quality_exact : smoothQuality(scale, m_meshPx);
    if (genie_build_mesh_ex(p, win, target, t.direction, &quality, m_mesh.get()) != 0) {
        return;
    }
    if (!m_debugDump.isEmpty()) {
        m_debugInfo = QStringLiteral(
                          "{\"p\": %1, \"frame\": [%2, %3, %4, %5], \"target\": [%6, %7, %8, %9], \"dir\": %10, "
                          "\"scale\": %11, \"mesh\": [%12, %13], \"icon\": [%14, %15, %16, %17], "
                          "\"panel\": [%18, %19, %20, %21]}")
                          .arg(p)
                          .arg(frame.x())
                          .arg(frame.y())
                          .arg(frame.width())
                          .arg(frame.height())
                          .arg(target.x0)
                          .arg(target.y0)
                          .arg(target.x1)
                          .arg(target.y1)
                          .arg(t.direction)
                          .arg(scale)
                          .arg(m_mesh->cols)
                          .arg(m_mesh->rows)
                          .arg(iconGeometry.x())
                          .arg(iconGeometry.y())
                          .arg(iconGeometry.width())
                          .arg(iconGeometry.height())
                          .arg(panelGeometry.x())
                          .arg(panelGeometry.y())
                          .arg(panelGeometry.width())
                          .arg(panelGeometry.height())
                          .toUtf8();
    }

    const WindowQuad &src = quads.first();
    const double ex = src[0].x(), ey = src[0].y();
    const double ew = src[2].x() - ex, eh = src[2].y() - ey;
    const double tu0 = src[0].u(), tv0 = src[0].v(), tdu = src[2].u() - tu0, tdv = src[2].v() - tv0;
    auto texU = [&](float u) {
        return tu0 + (double(u) - ex) / ew * tdu;
    };
    auto texV = [&](float v) {
        return tv0 + (double(v) - ey) / eh * tdv;
    };

    const genie_rect shadowRect{ex, ey, ew, eh};
    if (genie_build_shadow_mesh(m_mesh.get(), shadowRect, shadowRect, m_shadow.get()) != 0) {
        return;
    }
    const genie_shadow_mesh &m = *m_shadow;
    WindowQuadList out;
    out.reserve((m.cols - 1) * (m.rows - 1));
    m_weights.clear();
    m_clamp.clear();
    auto emit = [&](int r, int c, float clampFlag) {
        const genie_vertex *k[4] = {&m.v[r * m.cols + c], &m.v[r * m.cols + c + 1], &m.v[(r + 1) * m.cols + c + 1], &m.v[(r + 1) * m.cols + c]};
        WindowQuad q;
        float x[4], y[4], wgt[4];
        for (int i = 0; i < 4; ++i) {
            q[i] = WindowVertex(double(k[i]->x) - frame.x(), double(k[i]->y) - frame.y(), texU(k[i]->u), texV(k[i]->v));
            x[i] = k[i]->x;
            y[i] = k[i]->y;
        }
        caCornerWeights(x, y, wgt);
        m_weights.insert(m_weights.end(), wgt, wgt + 4);
        m_clamp.push_back(clampFlag);
        out.append(q);
    };

    for (int r = 0; r + 1 < m.rows; ++r) {
        for (int c = 0; c + 1 < m.cols; ++c) {
            const bool ring = r == 0 || c == 0 || r == m.rows - 2 || c == m.cols - 2;
            if (ring) {
                emit(r, c, 0.0f);
            }
        }
    }
    for (int r = 1; r + 2 < m.rows; ++r) {
        for (int c = 1; c + 2 < m.cols; ++c) {
            emit(r, c, 1.0f);
        }
    }
    quads = out;

    if (GLShader *shader = projectiveShader(); shader && m_weightsLocation >= 0) {
        ShaderBinder binder(shader);
        const int count = std::min<int>(m_weights.size() / 4, s_maxQuads);
        glUniform4fv(m_weightsLocation, count, m_weights.data());
        if (m_clampLocation >= 0) {
            glUniform1fv(m_clampLocation, count, m_clamp.data());
        }

        const double texW = ew * scale, texH = eh * scale;
        const float hu = float(0.5 / texW * tdu), hv = float(0.5 / texH * tdv);
        if (m_texMinLocation >= 0 && m_texMaxLocation >= 0) {
            glUniform2f(m_texMinLocation, float(texU(0.0f)) + hu, float(texV(0.0f)) + hv);
            glUniform2f(m_texMaxLocation, float(texU(float(frame.width()))) - hu, float(texV(float(frame.height()))) - hv);
        }
    }
}

}

#include "moc_genieeffect.cpp"
