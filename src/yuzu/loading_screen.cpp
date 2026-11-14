// SPDX-FileCopyrightText: Copyright 2026 Eden Emulator Project
// SPDX-License-Identifier: GPL-3.0-or-later

// SPDX-FileCopyrightText: Copyright 2019 yuzu Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <QBuffer>
#include <QByteArray>
#include <QGraphicsOpacityEffect>
#include <QIODevice>
#include <QImage>
#include <QPainter>
#include <QPixmap>
#include <QPropertyAnimation>
#include <QPainterPath>
#include <QPushButton>
#include <QRadialGradient>
#include <QStyleOption>
#include <QElapsedTimer>
#include <QTimer>
#include <cmath>
#include <functional>
#include "common/container/unordered_map.h"
#include "yuzu/util/calango_theme.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/loader/loader.h"
#include "ui_loading_screen.h"
#include "video_core/rasterizer_interface.h"
#include "video_core/shader_warm/shader_warm_builder.h"
#include "yuzu/loading_screen.h"
#include "yuzu/util/calango_backdrop.h"

// Mingw seems to not have QMovie at all. If QMovie is missing then use a single frame instead of an
// showing the full animation
#if !YUZU_QT_MOVIE_MISSING
#include <QMovie>
#endif

constexpr char PROGRESSBAR_STYLE_PREPARE[] = R"(
QProgressBar {}
QProgressBar::chunk {})";

constexpr char PROGRESSBAR_STYLE_BUILD[] = R"(
QProgressBar {
  background-color: black;
  border: 2px solid white;
  border-radius: 4px;
  padding: 2px;
}
QProgressBar::chunk {
  background-color: #ff3c28;
  width: 1px;
})";

constexpr char SKIP_BUTTON_STYLE[] = R"(
QPushButton {
  background-color: rgba(255, 200, 61, 40);
  color: #FFD966;
  border: 2px solid #FFC83D;
  border-radius: 17px;
  padding: 6px 28px;
  font-size: 12pt;
  font-weight: bold;
}
QPushButton:hover {
  background-color: #FFC83D;
  color: #2A1C00;
}
QPushButton:disabled {
  color: #7A705C;
  border-color: #4A3E2C;
  background-color: transparent;
})";

namespace {

const QColor Accent{0xFF, 0xC8, 0x3D};
const QColor AccentDeep{0xE8, 0x9E, 0x10};
const QColor Light{0xFF, 0xF4, 0xDA};

/// The loading bar: a slim track that fills with the accent colour, a fine pattern of
/// overlapping scales running through the fill and a sheen sweeping along it; while the length
/// is unknown a short segment glides to and fro.
class ScaleProgress : public QWidget {
public:
    explicit ScaleProgress(QWidget* parent) : QWidget{parent} {
        setMinimumHeight(64);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        setAttribute(Qt::WA_TranslucentBackground);
        // 60 frames a second; the fill glides to each new value instead of jumping.
        timer.setInterval(16);
        timer.setTimerType(Qt::PreciseTimer);
        clock.start();
        connect(&timer, &QTimer::timeout, this, [this] {
            const qreal dt = std::min(0.05, clock.restart() / 1000.0);
            phase += dt;
            const qreal target =
                maximum > minimum
                    ? std::clamp(double(value - minimum) / (maximum - minimum), 0.0, 1.0)
                    : 0.0;
            shown += (target - shown) * (1.0 - std::exp(-dt * 7.0));
            if (on_frame) {
                on_frame(dt); // the screen behind moves too
            } else {
                update();
            }
        });
    }

    /// Called every frame with the time since the last one (the loading screen steps its
    /// backdrop and repaints, this bar included).
    std::function<void(qreal)> on_frame;

    void SetRange(int min_value, int max_value) {
        if (max_value > min_value && maximum <= minimum) {
            shown = 0; // a new length: fill from the start
        }
        minimum = min_value;
        maximum = max_value;
        update();
    }
    void SetValue(int new_value) {
        value = new_value;
        update();
    }

protected:
    void showEvent(QShowEvent*) override {
        timer.start();
    }
    void hideEvent(QHideEvent*) override {
        timer.stop();
    }

    void paintEvent(QPaintEvent*) override {
        QPainter painter{this};
        painter.setRenderHint(QPainter::Antialiasing);
        const int width = std::min(this->width() - 80, 560);
        const qreal thickness = 12;
        const QRectF track{(this->width() - width) / 2.0, height() / 2.0 - thickness / 2,
                           double(width), thickness};
        const qreal radius = CalangoTheme::Radius(thickness / 2);
        painter.setPen(Qt::NoPen);
        painter.setBrush(CalangoTheme::Recolor(QColor(0x2A, 0x21, 0x15)));
        painter.drawRoundedRect(track, radius, radius);
        painter.setPen(QPen(QColor(255, 255, 255, 18), 1));
        painter.setBrush(Qt::NoBrush);
        painter.drawRoundedRect(track.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
        painter.setPen(Qt::NoPen);

        const bool busy = maximum <= minimum;
        QRectF fill;
        if (busy) {
            const qreal piece = track.width() * 0.28;
            const qreal swing = (1 - std::cos(phase * 2.2)) / 2;
            fill = {track.left() + (track.width() - piece) * swing, track.top(), piece,
                    track.height()};
        } else {
            fill = {track.left(), track.top(), std::max(track.height(), track.width() * shown),
                    track.height()};
        }
        QPainterPath shape;
        shape.addRoundedRect(fill, radius, radius);
        QLinearGradient gradient{fill.topLeft(), fill.bottomLeft()};
        gradient.setColorAt(0, CalangoTheme::Recolor(QColor(0xFF, 0xE2, 0x86)));
        gradient.setColorAt(0.55, CalangoTheme::Recolor(Accent));
        gradient.setColorAt(1, CalangoTheme::Recolor(AccentDeep));
        painter.setBrush(gradient);
        painter.drawPath(shape);

        painter.save();
        painter.setClipPath(shape);
        // Overlapping scales, row by row, drifting slowly to the right.
        const qreal scale = 7;
        const qreal drift = std::fmod(phase * 10, scale * 2);
        painter.setPen(QPen(QColor(0, 0, 0, 46), 1));
        painter.setBrush(Qt::NoBrush);
        for (int row = 0; row * scale * 0.6 < fill.height() + scale; ++row) {
            const qreal y = fill.top() + row * scale * 0.6;
            const qreal offset = (row % 2) ? scale : 0;
            for (qreal x = fill.left() - scale * 2 + drift + offset; x < fill.right() + scale;
                 x += scale * 2) {
                painter.drawArc(QRectF(x, y - scale, scale * 2, scale * 2), 200 * 16, 140 * 16);
            }
        }
        // A sheen sweeping along the fill.
        const qreal sweep = std::fmod(phase * 0.45, 1.0);
        const qreal sx = fill.left() - 80 + (fill.width() + 160) * sweep;
        QLinearGradient sheen{QPointF(sx - 60, 0), QPointF(sx + 60, 0)};
        sheen.setColorAt(0, QColor(255, 255, 255, 0));
        sheen.setColorAt(0.5, QColor(255, 255, 255, 60));
        sheen.setColorAt(1, QColor(255, 255, 255, 0));
        painter.setPen(Qt::NoPen);
        painter.setBrush(sheen);
        painter.drawRect(fill);
        painter.setBrush(QColor(255, 255, 255, 50));
        painter.drawRect(QRectF(fill.left(), fill.top(), fill.width(), 1.5));
        painter.restore();

        if (!busy) {
            QFont font = this->font();
            font.setWeight(QFont::DemiBold);
            font.setPixelSize(14);
            painter.setFont(font);
            painter.setPen(CalangoTheme::Recolor(Light));
            const int percent =
                int(100.0 * std::clamp(double(value - minimum) / (maximum - minimum), 0.0, 1.0));
            painter.drawText(QRectF(track.right() + 14, track.top() - 10, 70, track.height() + 20),
                             Qt::AlignVCenter | Qt::AlignLeft, QStringLiteral("%1%").arg(percent));
        }
    }

private:
    QTimer timer;
    qreal phase = 0;
    qreal shown = 0; ///< The filled share on screen, easing towards the real one
    QElapsedTimer clock;
    int minimum = 0;
    int maximum = 0;
    int value = 0;
};

} // Anonymous namespace

constexpr char PROGRESSBAR_STYLE_COMPLETE[] = R"(
QProgressBar {
  background-color: #0ab9e6;
  border: 2px solid white;
  border-radius: 4px;
  padding: 2px;
}
QProgressBar::chunk {
  background-color: #ff3c28;
})";

LoadingScreen::LoadingScreen(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::LoadingScreen>()),
      previous_stage(VideoCore::LoadCallbackStage::Complete) {
    ui->setupUi(this);
    setMinimumSize(Layout::MinimumSize::Width, Layout::MinimumSize::Height);

    // Calango Emulator's look: the background is painted (paintEvent), the labels sit on it.
    setStyleSheet(QString{});
    ui->fade_parent->setStyleSheet(QStringLiteral("background: transparent;"));
    ui->logo->setStyleSheet(QStringLiteral("background: transparent;"));
    ui->banner->setStyleSheet(QStringLiteral("background: transparent;"));
    ui->stage->setStyleSheet(QStringLiteral(
        "background: transparent; color: #FFF4DA; font-size: 18pt; font-weight: bold;"));
    ui->value->setStyleSheet(
        QStringLiteral("background: transparent; color: #E8D9B5; font-size: 12pt;"));
    // The progress bar's values drive the scale bar drawn in its place; its frames move the
    // backdrop too.
    scene = std::make_shared<CalangoBackdrop::Scene>();
    scale_progress = new ScaleProgress(this);
    static_cast<ScaleProgress*>(scale_progress)->on_frame = [this](qreal dt) {
        scene->Step(dt, QRectF(rect()));
        update();
    };
    ui->verticalLayout->insertWidget(ui->verticalLayout->indexOf(ui->progress_bar) + 1,
                                     scale_progress);
    ui->progress_bar->setMaximumHeight(0);
    ui->progress_bar->setVisible(false);

    // Create a fade out effect to hide this loading screen widget.
    // When fading opacity, it will fade to the parent widgets background color, which is why we
    // create an internal widget named fade_widget that we use the effect on, while keeping the
    // loading screen widget's background color black. This way we can create a fade to black effect
    opacity_effect = new QGraphicsOpacityEffect(this);
    opacity_effect->setOpacity(1);
    ui->fade_parent->setGraphicsEffect(opacity_effect);
    fadeout_animation = std::make_unique<QPropertyAnimation>(opacity_effect, "opacity");
    fadeout_animation->setDuration(500);
    fadeout_animation->setStartValue(1);
    fadeout_animation->setEndValue(0);
    fadeout_animation->setEasingCurve(QEasingCurve::OutBack);

    // After the fade completes, hide the widget and reset the opacity
    connect(fadeout_animation.get(), &QPropertyAnimation::finished, [this] {
        hide();
        opacity_effect->setOpacity(1);
        emit Hidden();
    });
    connect(this, &LoadingScreen::LoadProgress, this, &LoadingScreen::OnLoadProgress,
            Qt::QueuedConnection);

    // Shown while shaders are precached: starts the game without waiting for the rest.
    skip_button = new QPushButton(tr("Skip"), this);
    skip_button->setStyleSheet(CalangoTheme::Transform(QString::fromUtf8(SKIP_BUTTON_STYLE)));
    skip_button->setToolTip(tr("Start the game now. The shaders not built yet are built the next "
                               "time the game starts, or while you play when the game needs "
                               "them."));
    skip_button->hide();
    ui->verticalLayout->insertWidget(2, skip_button, 0, Qt::AlignHCenter | Qt::AlignTop);
    connect(skip_button, &QPushButton::clicked, this, [this] {
        VideoCommon::ShaderWarm::RequestSkipWarm();
        skip_button->setEnabled(false);
    });
    qRegisterMetaType<VideoCore::LoadCallbackStage>();

    stage_translations = {
        {VideoCore::LoadCallbackStage::Prepare, tr("Getting your game ready...")},
        {VideoCore::LoadCallbackStage::Build, tr("Loading saved shaders %1 / %2")},
        {VideoCore::LoadCallbackStage::Complete, tr("Here we go!")},
        {VideoCore::LoadCallbackStage::Warm, tr("Building shaders %1 / %2 (just this once)")},
    };
    progressbar_style = {
        {VideoCore::LoadCallbackStage::Prepare, PROGRESSBAR_STYLE_PREPARE},
        {VideoCore::LoadCallbackStage::Build, PROGRESSBAR_STYLE_BUILD},
        {VideoCore::LoadCallbackStage::Complete, PROGRESSBAR_STYLE_COMPLETE},
        {VideoCore::LoadCallbackStage::Warm, PROGRESSBAR_STYLE_BUILD},
    };
}

LoadingScreen::~LoadingScreen() = default;

void LoadingScreen::Prepare(Loader::AppLoader& loader) {
    std::vector<u8> buffer;
    if (loader.ReadBanner(buffer) == Loader::ResultStatus::Success) {
#ifdef YUZU_QT_MOVIE_MISSING
        QPixmap map;
        map.loadFromData(buffer.data(), buffer.size());
        ui->banner->setPixmap(map);
#else
        backing_mem = std::make_unique<QByteArray>(reinterpret_cast<char*>(buffer.data()),
                                                   static_cast<int>(buffer.size()));
        backing_buf = std::make_unique<QBuffer>(backing_mem.get());
        backing_buf->open(QIODevice::ReadOnly);
        animation = std::make_unique<QMovie>(backing_buf.get(), QByteArray());
        animation->start();
        ui->banner->setMovie(animation.get());
#endif
        buffer.clear();
    }
    // The game's icon, drawn as a big tile over the stage text (paintEvent).
    game_icon = {};
    if (std::vector<u8> icon; loader.ReadIcon(icon) == Loader::ResultStatus::Success) {
        game_icon.loadFromData(icon.data(), static_cast<uint>(icon.size()));
    }
    ambient = {}; // made at the screen's size when painted
    if (loader.ReadLogo(buffer) == Loader::ResultStatus::Success) {
        QPixmap map;
        map.loadFromData(buffer.data(), static_cast<uint>(buffer.size()));
        ui->logo->setPixmap(map);
    }

    slow_shader_compile_start = false;
    OnLoadProgress(VideoCore::LoadCallbackStage::Prepare, 0, 0);
}

void LoadingScreen::OnLoadComplete() {
    fadeout_animation->start(QPropertyAnimation::KeepWhenStopped);
}

void LoadingScreen::OnLoadProgress(VideoCore::LoadCallbackStage stage, std::size_t value,
                                   std::size_t total) {
    using namespace std::chrono;
    const auto now = steady_clock::now();
    // reset the timer if the stage changes
    if (stage != previous_stage) {
        // The scale bar shows from the first stage on; the old bar stays hidden.
        scale_progress->setVisible(true);
        skip_button->setVisible(stage == VideoCore::LoadCallbackStage::Warm);
        skip_button->setEnabled(true);
        previous_stage = stage;
        // reset back to fast shader compiling since the stage changed
        slow_shader_compile_start = false;
    }
    // update the max of the progress bar if the number of shaders change
    if (total != previous_total) {
        ui->progress_bar->setMaximum(static_cast<int>(total));
        previous_total = total;
    }
    // Reset the progress bar ranges if compilation is done
    if (stage == VideoCore::LoadCallbackStage::Complete) {
        ui->progress_bar->setRange(0, 0);
    }

    QString estimate;
    // If there's a drastic slowdown in the rate, then display an estimate
    if (now - previous_time > milliseconds{50} || slow_shader_compile_start) {
        if (!slow_shader_compile_start) {
            slow_shader_start = steady_clock::now();
            slow_shader_compile_start = true;
            slow_shader_first_value = value;
        }
        // only calculate an estimate time after a second has passed since stage change
        const auto diff = duration_cast<milliseconds>(now - slow_shader_start);
        if (diff > seconds{1} && value > slow_shader_first_value) {
            const auto eta_mseconds =
                static_cast<long>(static_cast<double>(total - slow_shader_first_value) /
                                  (value - slow_shader_first_value) * diff.count());
            estimate =
                tr("Estimated Time %1")
                    .arg(QTime(0, 0, 0, 0)
                             .addMSecs(std::max<long>(eta_mseconds - diff.count() + 1000, 1000))
                             .toString(QStringLiteral("mm:ss")));
        }
    }

    // update labels and progress bar
    if (stage == VideoCore::LoadCallbackStage::Warm && total == 0) {
        // Before the count is known: the game's files are read for their shader programs.
        ui->stage->setText(tr("Gathering this game's shaders (once, about a minute)..."));
        ui->progress_bar->setRange(0, 0);
    } else if (stage == VideoCore::LoadCallbackStage::Build ||
               stage == VideoCore::LoadCallbackStage::Warm) {
        ui->stage->setText(stage_translations[stage].arg(value).arg(total));
    } else {
        ui->stage->setText(stage_translations[stage]);
    }
    ui->value->setText(estimate);
    ui->progress_bar->setValue(static_cast<int>(value));
    auto* const bar = static_cast<ScaleProgress*>(scale_progress);
    if (stage == VideoCore::LoadCallbackStage::Prepare) {
        bar->SetRange(0, 0); // no length known yet: the gliding piece
    } else {
        bar->SetRange(ui->progress_bar->minimum(), ui->progress_bar->maximum());
    }
    bar->SetValue(static_cast<int>(value));
    previous_time = now;

    // TEST: CALANGO_LOADING_SHOT=<png> saves a picture of the loading screen 3 s in.
    if (static bool shot_taken = false; !shot_taken) {
        if (const QByteArray shot = qgetenv("CALANGO_LOADING_SHOT"); !shot.isEmpty()) {
            shot_taken = true;
            QTimer::singleShot(3000, this, [this, path = QString::fromUtf8(shot)] {
                window()->grab().save(path);
            });
        }
    }
}

void LoadingScreen::paintEvent(QPaintEvent* event) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QRect area = rect();
    // The home view's moving backdrop, with the game's icon as the ambient glow.
    if (ambient.size() != area.size() && !game_icon.isNull()) {
        ambient = CalangoBackdrop::MakeAmbient(game_icon, area.size());
    }
    scene->Paint(p, area, ambient, 0.45);

    // The game's icon as a glowing rounded tile above the stage text.
    if (!game_icon.isNull()) {
        const int tile = std::clamp(area.height() / 4, 96, 220);
        const QPoint stage_top = ui->stage->mapTo(this, QPoint(0, 0));
        const QRectF tile_rect{(area.width() - tile) / 2.0,
                               std::max(24.0, stage_top.y() - tile - 28.0), double(tile),
                               double(tile)};
        const qreal radius = tile * 0.12;
        for (int i = 6; i >= 1; --i) {
            QColor glow{0xFF, 0xC8, 0x3D};
            glow.setAlpha(12 + (6 - i) * 6);
            p.setPen(QPen(glow, 2));
            p.setBrush(Qt::NoBrush);
            const qreal spread = 3.0 + i * 2.5;
            p.drawRoundedRect(tile_rect.adjusted(-spread, -spread, spread, spread),
                              radius + spread, radius + spread);
        }
        p.save();
        QPainterPath clip;
        clip.addRoundedRect(tile_rect, radius, radius);
        p.setClipPath(clip);
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        p.drawPixmap(tile_rect.toRect(), game_icon);
        p.restore();
        p.setPen(QPen(CalangoTheme::Recolor(QColor(0xFF, 0xC8, 0x3D)), 4));
        p.setBrush(Qt::NoBrush);
        p.drawRoundedRect(tile_rect, radius, radius);
    }
    QWidget::paintEvent(event);
}

void LoadingScreen::Clear() {
#ifndef YUZU_QT_MOVIE_MISSING
    animation.reset();
    backing_buf.reset();
    backing_mem.reset();
#endif
}
