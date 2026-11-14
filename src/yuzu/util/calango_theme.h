// SPDX-FileCopyrightText: 2026 CheezyEmu contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>

#include <QColor>
#include <QRegularExpression>
#include <QString>

#include "common/common_types.h"

/// The Customize menu's choices for the Calango Emulator theme: an accent colour, a background
/// colour and a button shape. The stylesheet and the parts the emulator draws itself are written
/// in a warm reference palette (ReferenceAccent, ReferenceBackground); Recolor maps every warm
/// accent colour to the chosen accent, every warm dark to the chosen background and the warm
/// light greys to a cool neutral. The stylesheet goes through Transform when it loads; the drawn
/// parts (glass, capsules, the home screen's light) through Recolor and Radius, and redraw when
/// Generation changes.
namespace CalangoTheme {

/// The palette the stylesheet is written in.
inline const QColor ReferenceAccent{0xFF, 0xC8, 0x3D};
inline const QColor ReferenceBackground{0x24, 0x1B, 0x12};

/// Calango's own look: jade green on dark basalt.
inline const QColor DefaultAccent{0x3F, 0xBF, 0x7F};
inline const QColor DefaultBackground{0x12, 0x18, 0x15};

enum class Shape : u32 { Pill = 0, Soft = 1, Square = 2 };

namespace Detail {
inline QColor accent = DefaultAccent;
inline QColor background = DefaultBackground;
inline Shape shape = Shape::Pill;
inline std::atomic<u32> generation{0};
} // namespace Detail

/// Takes the choices (Customize menu, settings at startup).
inline void Configure(const QColor& accent, const QColor& background, Shape shape) {
    Detail::accent = accent.isValid() ? accent : DefaultAccent;
    Detail::background = background.isValid() ? background : DefaultBackground;
    Detail::shape = shape;
    Detail::generation.fetch_add(1, std::memory_order_relaxed);
}

/// Changes whenever the choices do: cached drawings compare it.
[[nodiscard]] inline u32 Generation() {
    return Detail::generation.load(std::memory_order_relaxed);
}

[[nodiscard]] inline bool IsDefault() {
    return Detail::accent == ReferenceAccent && Detail::background == ReferenceBackground;
}

/// A colour of the theme as the choices have it.
[[nodiscard]] inline QColor Recolor(const QColor& color) {
    if (IsDefault()) {
        return color;
    }
    float hue = 0.0f;
    float saturation = 0.0f;
    float value = 0.0f;
    float alpha = 0.0f;
    color.getHsvF(&hue, &saturation, &value, &alpha);
    const float degrees = hue < 0.0f ? -1.0f : hue * 360.0f;
    const bool warm = degrees >= 12.0f && degrees <= 68.0f;
    const auto shifted = [&](const QColor& to, const QColor& from) {
        float to_h = 0, to_s = 0, to_v = 0, from_h = 0, from_s = 0, from_v = 0;
        to.getHsvF(&to_h, &to_s, &to_v);
        from.getHsvF(&from_h, &from_s, &from_v);
        if (to_h < 0.0f) {
            to_h = from_h; // a grey: keep the hue, it loses its saturation below
        }
        float new_h = std::fmod(to_h + (hue - from_h) + 1.0f, 1.0f);
        const float new_s = std::clamp(saturation * (to_s / std::max(from_s, 0.01f)), 0.0f, 1.0f);
        const float new_v = std::clamp(value * (to_v / std::max(from_v, 0.01f)), 0.0f, 1.0f);
        return QColor::fromHsvF(new_h, new_s, new_v, alpha);
    };
    if (warm && saturation >= 0.18f && value >= 0.35f) {
        return shifted(Detail::accent, ReferenceAccent);
    }
    if (warm && saturation >= 0.08f && value < 0.35f) {
        return shifted(Detail::background, ReferenceBackground);
    }
    if (warm && value >= 0.35f) {
        // Creams and warm light greys (text, quiet lines): a cool neutral with a trace of the
        // accent's hue.
        float accent_h = 0, accent_s = 0, accent_v = 0;
        Detail::accent.getHsvF(&accent_h, &accent_s, &accent_v);
        return QColor::fromHsvF(accent_h < 0.0f ? 0.0f : accent_h, saturation * 0.35f, value,
                                alpha);
    }
    return color;
}

/// A corner radius as the button shape has it.
[[nodiscard]] inline qreal Radius(qreal radius) {
    switch (Detail::shape) {
    case Shape::Soft:
        return std::min(radius, qreal{6});
    case Shape::Square:
        return std::min(radius, qreal{2});
    case Shape::Pill:
        break;
    }
    return radius;
}

/// The Calango Emulator stylesheet with the choices applied: every #rrggbb, rgb() and rgba() colour
/// through Recolor, every border radius through Radius.
[[nodiscard]] inline QString Transform(const QString& stylesheet) {
    if (IsDefault() && Detail::shape == Shape::Pill) {
        return stylesheet;
    }
    QString result;
    result.reserve(stylesheet.size());
    static const QRegularExpression pattern{
        QStringLiteral(R"(#([0-9A-Fa-f]{6})\b|rgba?\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*(?:,\s*([0-9.]+)\s*)?\)|((?:border(?:-top-left|-top-right|-bottom-left|-bottom-right)?-radius)\s*:\s*)(\d+)px)")};
    qsizetype last = 0;
    auto it = pattern.globalMatch(stylesheet);
    while (it.hasNext()) {
        const QRegularExpressionMatch match = it.next();
        result.append(QStringView{stylesheet}.mid(last, match.capturedStart() - last));
        last = match.capturedEnd();
        if (!match.captured(1).isEmpty()) {
            const QColor color = Recolor(QColor(QLatin1Char('#') + match.captured(1)));
            result.append(color.name(QColor::HexRgb));
        } else if (!match.captured(2).isEmpty()) {
            const bool has_alpha = !match.captured(5).isEmpty();
            const QColor color = Recolor(QColor(match.captured(2).toInt(),
                                                match.captured(3).toInt(),
                                                match.captured(4).toInt()));
            if (has_alpha) {
                result.append(QStringLiteral("rgba(%1, %2, %3, %4)")
                                  .arg(color.red())
                                  .arg(color.green())
                                  .arg(color.blue())
                                  .arg(match.captured(5)));
            } else {
                result.append(QStringLiteral("rgb(%1, %2, %3)")
                                  .arg(color.red())
                                  .arg(color.green())
                                  .arg(color.blue()));
            }
        } else {
            const qreal radius = Radius(match.captured(7).toDouble());
            result.append(match.captured(6));
            result.append(QStringLiteral("%1px").arg(qRound(radius)));
        }
    }
    result.append(QStringView{stylesheet}.mid(last));
    return result;
}

} // namespace CalangoTheme
