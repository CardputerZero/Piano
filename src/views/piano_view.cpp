#include "views/piano_view.hpp"

#include "assets/font_assets.hpp"

#include <core/animation/animate/animate.hpp>
#include <core/easing/ease.hpp>
#include <lvgl/lvgl_cpp/canvas.hpp>
#include <lvgl/lvgl_cpp/label.hpp>
#include <lvgl/lvgl_cpp/obj.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace piano {
namespace {

constexpr int kScreenWidth  = 320;
constexpr int kScreenHeight = 170;

constexpr int kWhiteKeyY      = 17;
constexpr int kWhiteKeyWidth  = 34;
constexpr int kWhiteKeyHeight = 147;
constexpr int kBlackKeyY      = 17;
constexpr int kBlackKeyWidth  = 26;
constexpr int kBlackKeyHeight = 86;
constexpr int kKeyRadius      = 4;
constexpr int kNoteLabelY     = 142;

constexpr int kChordKeyLabelY   = kNoteLabelY - kWhiteKeyY;
constexpr int kChordPanelX      = 72;
constexpr int kChordPanelY      = 29;
constexpr int kChordPanelWidth  = 176;
constexpr int kChordPanelHeight = 39;
constexpr int kChordPanelRadius = 5;

constexpr int kMapBadgeWidth  = 20;
constexpr int kMapBadgeHeight = 20;
constexpr int kMapBadgeRadius = 6;

constexpr int kPlayalongPanelX      = 228;
constexpr int kPlayalongPanelY      = 10;
constexpr int kPlayalongPanelWidth  = 103;
constexpr int kPlayalongPanelHeight = 43;
constexpr int kPlayalongPanelRadius = 10;
constexpr int kPlayalongBadgeX      = 5;
constexpr int kPlayalongBadgeY      = 16;
constexpr int kPlayalongLabelX      = 30;
constexpr int kPlayalongLabelY      = 19;
constexpr int kTargetBadgeLift      = 5;

constexpr float kTau = 6.28318530717958647692f;

constexpr uint32_t kBackgroundColor        = 0x000000;
constexpr uint32_t kWhiteKeyColor          = 0xFFFFFF;
constexpr uint32_t kWhiteKeyPressedColor   = 0xDADADA;
constexpr uint32_t kBlackKeyColor          = 0x282828;
constexpr uint32_t kBlackKeyPressedColor   = 0x484848;
constexpr uint32_t kArrowColor             = 0xFF6633;
constexpr lv_opa_t kArrowPressedOpacity    = 150;
constexpr lv_opa_t kArrowReleasedOpacity   = static_cast<lv_opa_t>(LV_OPA_COVER);
constexpr uint32_t kHeaderColor            = 0x464545;
constexpr uint32_t kHeaderDividerColor     = 0x363636;
constexpr uint32_t kHeaderTextColor        = 0xC2C2C2;
constexpr uint32_t kKeymapHintColor        = 0xFED40D;
constexpr uint32_t kNoteTextColor          = 0xDBDBDB;
constexpr uint32_t kChordPanelColor        = 0x1D1D1D;
constexpr uint32_t kChordPanelPressedColor = 0x292929;
constexpr uint32_t kChordPanelBorderColor  = 0x4A4A4A;
constexpr uint32_t kChordTitleColor        = 0xF0F0F0;
constexpr uint32_t kChordDetailColor       = 0xA7A7A7;
constexpr uint32_t kChordActiveColor       = 0xFF9A58;
constexpr uint32_t kWhiteBadgeColor        = 0xD7D7D7;
constexpr uint32_t kWhiteBadgeBorderColor  = 0x9F9F9F;
constexpr uint32_t kWhiteBadgeTextColor    = 0x6B6B6B;
constexpr uint32_t kBlackBadgeColor        = 0x595959;
constexpr uint32_t kBlackBadgeBorderColor  = 0x838383;
constexpr uint32_t kBlackBadgeTextColor    = 0xC2C2C2;
constexpr uint32_t kPlayalongActiveColor   = 0x66E39A;
constexpr uint32_t kTargetBadgeColor       = kKeymapHintColor;
constexpr uint32_t kTargetBadgeTextColor   = 0x5A4A00;
constexpr uint32_t kDemoBadgeColor         = 0x168DFF;
constexpr uint32_t kDemoBadgeBorderColor   = 0x72C9FF;
constexpr uint32_t kDemoBadgeTextColor     = 0xFFFFFF;

constexpr std::array<int, 8> kConfettiDx{-24, -17, -8, 7, 16, 24, -20, 20};
constexpr std::array<int, 8> kConfettiDy{-7, -20, -25, -25, -19, -6, 8, 8};
constexpr std::array<uint32_t, 6> kConfettiColors{
    0xF4CC4B, 0x4BC2E2, 0xF06598, 0x49C17D, 0xB974DF, 0xEF9850,
};

constexpr std::array<int, 8> kWhiteKeyX{17, 53, 89, 125, 161, 197, 233, 269};
constexpr std::array<int, 8> kWhiteNoteIndex{0, 2, 4, 5, 7, 9, 11, 12};
constexpr std::array<const char*, 8> kWhiteKeymap{"A", "S", "D", "F", "G", "H", "J", "K"};

constexpr std::array<int, 5> kBlackKeyX{39, 75, 147, 183, 219};
constexpr std::array<int, 5> kBlackNoteIndex{1, 3, 6, 8, 10};
constexpr std::array<const char*, 5> kBlackKeymap{"W", "E", "T", "Y", "U"};

using smooth_ui_toolkit::lvgl_cpp::Canvas;
using smooth_ui_toolkit::lvgl_cpp::Container;
using smooth_ui_toolkit::lvgl_cpp::Label;

enum class BadgeHighlight {
    None,
    Guide,
    Demo,
};

void setupContainer(Container& container, lv_opa_t background_opacity = LV_OPA_COVER)
{
    container.setBgOpa(background_opacity);
    container.setBorderWidth(0);
    container.setOutlineWidth(0);
    container.setShadowWidth(0);
    container.setRadius(0);
    container.setPaddingAll(0);
    container.setScrollbarMode(LV_SCROLLBAR_MODE_OFF);
    container.removeFlag(LV_OBJ_FLAG_SCROLLABLE);
}

void setupCanvas(Canvas& canvas)
{
    canvas.setBgOpa(LV_OPA_TRANSP);
    canvas.setBorderWidth(0);
    canvas.setOutlineWidth(0);
    canvas.setShadowWidth(0);
    canvas.setPaddingAll(0);
    canvas.removeFlag(LV_OBJ_FLAG_SCROLLABLE);
}

}  // namespace

struct PianoView::PlayalongEffects {
    explicit PlayalongEffects(lv_obj_t* parent)
    {
        for (std::size_t index = 0; index < confetti.size(); ++index) {
            auto& piece        = confetti[index];
            piece              = std::make_unique<Container>(parent);
            const int diameter = index % 2 == 0 ? 5 : 3;
            piece->setSize(diameter, diameter);
            setupContainer(*piece);
            piece->setBgColor(lv_color_hex(kConfettiColors[index % kConfettiColors.size()]));
            piece->setRadius(LV_RADIUS_CIRCLE);
            piece->setHidden(true);
        }

        target_bob.start                          = 0.0f;
        target_bob.end                            = 1.0f;
        target_bob.repeat                         = -1;
        target_bob.repeatType                     = smooth_ui_toolkit::AnimateRepeatType::Reverse;
        target_bob.springOptions().visualDuration = 0.58f;
        target_bob.springOptions().bounce         = 0.15f;
        target_bob.onUpdate([this](float value) { target_bob_value = value; });
        target_bob.init();
        target_bob.cancel();

        success_burst.start                          = 0.0f;
        success_burst.end                            = 1.0f;
        success_burst.easingOptions().duration       = 0.62f;
        success_burst.easingOptions().easingFunction = smooth_ui_toolkit::ease::linear;
        success_burst.onUpdate([this](float value) { success_progress = value; });
        success_burst.onComplete([this]() {
            success_active = false;
            success_target = -1;
        });
        success_burst.init();
        success_burst.cancel();

        demo_beat.start                          = 0.0f;
        demo_beat.end                            = 1.0f;
        demo_beat.easingOptions().duration       = 0.34f;
        demo_beat.easingOptions().easingFunction = smooth_ui_toolkit::ease::linear;
        demo_beat.onUpdate([this](float value) { demo_beat_progress = value; });
        demo_beat.onComplete([this]() { demo_beat_active = false; });
        demo_beat.init();
        demo_beat.cancel();
    }

    void tick(uint32_t now_ms)
    {
        const float now_seconds = static_cast<float>(now_ms) / 1000.0f;
        if (target_bob_active) {
            target_bob.update(now_seconds);
        }
        if (success_active) {
            success_burst.update(now_seconds);
        }
        if (demo_beat_active) {
            demo_beat.update(now_seconds);
        }
        renderConfetti();
    }

    void celebrate(int target, int origin_x, int origin_y)
    {
        success_burst.cancel();
        success_target   = target;
        success_origin_x = origin_x;
        success_origin_y = origin_y;
        success_progress = 0.0f;
        success_active   = true;
        success_burst.init();
        success_burst.play();
    }

    void cancelCelebration()
    {
        success_burst.cancel();
        success_active = false;
        success_target = -1;
        hideConfetti();
    }

    void startGuideBob()
    {
        target_bob.cancel();
        target_bob.start  = 0.0f;
        target_bob.end    = 1.0f;
        target_bob_value  = 0.0f;
        target_bob_active = true;
        target_bob.init();
        target_bob.play();
    }

    void cancelGuideBob()
    {
        target_bob.cancel();
        target_bob_value  = 0.0f;
        target_bob_active = false;
    }

    void pulseDemoBeat()
    {
        demo_beat.cancel();
        demo_beat_progress = 0.0f;
        demo_beat_active   = true;
        demo_beat.init();
        demo_beat.play();
    }

    void cancelDemoBeat()
    {
        demo_beat.cancel();
        demo_beat_progress = 0.0f;
        demo_beat_active   = false;
    }

    void stop()
    {
        cancelGuideBob();
        cancelCelebration();
        cancelDemoBeat();
    }

    int targetLift() const
    {
        if (!target_bob_active) {
            return 0;
        }
        return static_cast<int>(std::lround(std::clamp(target_bob_value, 0.0f, 1.0f) * kTargetBadgeLift));
    }

    int celebratingTarget() const
    {
        return success_active ? success_target : -1;
    }

    int successLift() const
    {
        return success_active ? static_cast<int>(std::lround(
                                    std::sin(std::clamp(success_progress, 0.0f, 1.0f) * kTau * 0.5f) * 4.0f))
                              : 0;
    }

    int successScale() const
    {
        if (!success_active) {
            return 256;
        }

        const float progress = std::clamp(success_progress, 0.0f, 1.0f);
        float scale          = 1.0f;
        if (progress < 0.28f) {
            scale = 0.82f + 0.40f * smooth_ui_toolkit::ease::ease_out_back(progress / 0.28f);
        } else if (progress < 0.56f) {
            scale = 1.22f - 0.28f * smooth_ui_toolkit::ease::ease_out_quad((progress - 0.28f) / 0.28f);
        } else {
            scale = 0.94f + 0.06f * smooth_ui_toolkit::ease::ease_out_back((progress - 0.56f) / 0.44f);
        }
        return static_cast<int>(std::lround(scale * 256.0f));
    }

    int demoBeatLift() const
    {
        if (!demo_beat_active) {
            return 0;
        }
        const float progress = std::clamp(demo_beat_progress, 0.0f, 1.0f);
        return static_cast<int>(std::lround(std::sin(progress * kTau * 0.5f) * 3.0f));
    }

    std::array<std::unique_ptr<Container>, 8> confetti;
    smooth_ui_toolkit::Animate target_bob;
    smooth_ui_toolkit::Animate success_burst;
    smooth_ui_toolkit::Animate demo_beat;
    float target_bob_value   = 0.0f;
    float success_progress   = 1.0f;
    float demo_beat_progress = 0.0f;
    bool success_active      = false;
    bool demo_beat_active    = false;
    bool target_bob_active   = false;
    int success_target       = -1;
    int success_origin_x     = 0;
    int success_origin_y     = 0;
    bool confetti_visible    = false;

private:
    void hideConfetti()
    {
        if (!confetti_visible) {
            return;
        }
        for (auto& piece : confetti) {
            piece->setHidden(true);
        }
        confetti_visible = false;
    }

    void renderConfetti()
    {
        const float progress = std::clamp(success_progress, 0.0f, 1.0f);
        const bool visible   = success_active && progress > 0.03f && progress < 0.94f;
        const float burst    = std::sin(std::min(progress / 0.72f, 1.0f) * kTau * 0.25f);
        const float fade     = 1.0f - std::clamp((progress - 0.42f) / 0.52f, 0.0f, 1.0f);

        if (!visible) {
            hideConfetti();
            return;
        }
        if (!confetti_visible) {
            for (auto& piece : confetti) {
                piece->setHidden(false);
            }
            confetti_visible = true;
        }

        for (std::size_t index = 0; index < confetti.size(); ++index) {
            auto& piece      = confetti[index];
            const int radius = (index % 2 == 0 ? 5 : 3) / 2;
            piece->setPos(success_origin_x +
                              static_cast<int>(std::lround(static_cast<float>(kConfettiDx[index]) * burst)) - radius,
                          success_origin_y +
                              static_cast<int>(std::lround(static_cast<float>(kConfettiDy[index]) * burst +
                                                           progress * progress * 14.0f)) -
                              radius);
            piece->setOpa(static_cast<lv_opa_t>(std::lround(fade * static_cast<float>(LV_OPA_COVER))));
        }
    }
};

struct PianoView::KeyVisual {
    KeyVisual(lv_obj_t* parent, int x, int y, int width, int height, bool black, int note_index, int chord_index,
              const char* keymap_text)
        : note_index(note_index),
          chord_index(chord_index),
          black(black),
          body_x(x),
          body_y(y),
          badge_x(black ? 3 : 7),
          badge_y(black ? 58 : 91),
          body(std::make_unique<Container>(parent)),
          badge(std::make_unique<Container>(body->raw_ptr())),
          label(std::make_unique<Label>(badge->raw_ptr())),
          chord_label(black ? nullptr : std::make_unique<Label>(body->raw_ptr()))
    {
        body->setSize(width, height);
        body->setPos(x, y);
        setupContainer(*body);
        body->setBgColor(lv_color_hex(black ? kBlackKeyColor : kWhiteKeyColor));
        body->setRadius(kKeyRadius);

        badge->setSize(kMapBadgeWidth, kMapBadgeHeight);
        badge->setPos(badge_x, badge_y);
        setupContainer(*badge);
        badge->setBgColor(lv_color_hex(black ? kBlackBadgeColor : kWhiteBadgeColor));
        badge->setBorderWidth(1);
        badge->setBorderColor(lv_color_hex(black ? kBlackBadgeBorderColor : kWhiteBadgeBorderColor));
        badge->setRadius(kMapBadgeRadius);
        badge->setTransformPivot(kMapBadgeWidth / 2, kMapBadgeHeight / 2);
        badge->setHidden(true);

        label->setSize(kMapBadgeWidth, LV_SIZE_CONTENT);
        label->align(LV_ALIGN_CENTER, 0, 0);
        label->setTextAlign(LV_TEXT_ALIGN_CENTER);
        label->setTextFont(uiFont14());
        label->setTextColor(lv_color_hex(black ? kBlackBadgeTextColor : kWhiteBadgeTextColor));
        label->setText(keymap_text);

        if (chord_label) {
            chord_label->setSize(width, LV_SIZE_CONTENT);
            chord_label->setPos(0, kChordKeyLabelY);
            chord_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
            chord_label->setLongMode(LV_LABEL_LONG_CLIP);
            chord_label->setTextColor(lv_color_hex(kNoteTextColor));
            chord_label->setHidden(true);
        }
    }

    void render(const PianoState& state)
    {
        const bool chord_mode = state.mode == PianoMode::Chord;
        const bool pressed    = chord_mode ? chord_index >= 0 && state.active_chord_index == chord_index
                                           : state.pressed_notes[static_cast<std::size_t>(note_index)];

        body->setHidden(chord_mode && black);
        if (!black) {
            chord_label->setHidden(!chord_mode);
            if (chord_mode) {
                const std::string text = chordLabel(state.tonic, state.tonality, static_cast<std::size_t>(chord_index));
                const bool compact     = text.size() > 2;
                chord_label->setTextFont(compact ? uiFont10() : uiFont14());
                chord_label->setY(kChordKeyLabelY + (compact ? 2 : 0));
                chord_label->setText(text);
            }
        }

        body->setBgColor(lv_color_hex(black ? (pressed ? kBlackKeyPressedColor : kBlackKeyColor)
                                            : (pressed ? kWhiteKeyPressedColor : kWhiteKeyColor)));
        applyKeymapVisual(state.keymap_visible, LV_OPA_COVER, BadgeHighlight::None, 0, 256);
    }

    void applyKeymapVisual(bool visible, lv_opa_t opacity, BadgeHighlight highlight, int lift, int scale)
    {
        if (!keymap_visual_initialized || keymap_visible != visible) {
            badge->setHidden(!visible);
            keymap_visible = visible;
        }
        if (!keymap_visual_initialized || keymap_opacity != opacity) {
            badge->setOpa(opacity);
            keymap_opacity = opacity;
        }
        if (!keymap_visual_initialized || keymap_lift != lift) {
            badge->setY(badge_y - lift);
            keymap_lift = lift;
        }
        if (!keymap_visual_initialized || keymap_highlight != highlight) {
            uint32_t background = black ? kBlackBadgeColor : kWhiteBadgeColor;
            uint32_t border     = black ? kBlackBadgeBorderColor : kWhiteBadgeBorderColor;
            uint32_t text       = black ? kBlackBadgeTextColor : kWhiteBadgeTextColor;
            if (highlight == BadgeHighlight::Guide) {
                background = kTargetBadgeColor;
                border     = kTargetBadgeColor;
                text       = kTargetBadgeTextColor;
            } else if (highlight == BadgeHighlight::Demo) {
                background = kDemoBadgeColor;
                border     = kDemoBadgeBorderColor;
                text       = kDemoBadgeTextColor;
            }
            badge->setBgColor(lv_color_hex(background));
            badge->setBorderColor(lv_color_hex(border));
            label->setTextColor(lv_color_hex(text));
            keymap_highlight = highlight;
        }
        if (!keymap_visual_initialized || keymap_scale != scale) {
            lv_obj_set_style_transform_scale_x(badge->raw_ptr(), scale, LV_PART_MAIN);
            lv_obj_set_style_transform_scale_y(badge->raw_ptr(), scale, LV_PART_MAIN);
            keymap_scale = scale;
        }
        keymap_visual_initialized = true;
    }

    int badgeCenterX() const
    {
        return body_x + badge_x + kMapBadgeWidth / 2;
    }
    int badgeCenterY() const
    {
        return body_y + badge_y + kMapBadgeHeight / 2;
    }

    int note_index;
    int chord_index;
    bool black;
    int body_x;
    int body_y;
    int badge_x;
    int badge_y;
    bool keymap_visual_initialized  = false;
    bool keymap_visible             = false;
    BadgeHighlight keymap_highlight = BadgeHighlight::None;
    lv_opa_t keymap_opacity         = LV_OPA_TRANSP;
    int keymap_lift                 = 0;
    int keymap_scale                = 256;
    std::unique_ptr<Container> body;
    std::unique_ptr<Container> badge;
    std::unique_ptr<Label> label;
    std::unique_ptr<Label> chord_label;
};

PianoView::PianoView(PianoViewModel& view_model) : _view_model(view_model)
{
}

PianoView::~PianoView()
{
    onExit();
}

void PianoView::onEnter(lv_obj_t* parent)
{
    if (_root || !parent) {
        return;
    }

    _root = std::make_unique<Container>(parent);
    _root->setSize(kScreenWidth, kScreenHeight);
    _root->setPos(0, 0);
    setupContainer(*_root);
    _root->setBgColor(lv_color_hex(kBackgroundColor));

    for (std::size_t index = 0; index < _white_keys.size(); ++index) {
        _white_keys[index] = std::make_unique<KeyVisual>(_root->raw_ptr(), kWhiteKeyX[index], kWhiteKeyY,
                                                         kWhiteKeyWidth, kWhiteKeyHeight, false, kWhiteNoteIndex[index],
                                                         static_cast<int>(index), kWhiteKeymap[index]);
    }
    for (std::size_t index = 0; index < _black_keys.size(); ++index) {
        _black_keys[index] =
            std::make_unique<KeyVisual>(_root->raw_ptr(), kBlackKeyX[index], kBlackKeyY, kBlackKeyWidth,
                                        kBlackKeyHeight, true, kBlackNoteIndex[index], -1, kBlackKeymap[index]);
    }

    _low_note_label = std::make_unique<Label>(_root->raw_ptr());
    _low_note_label->setSize(kWhiteKeyWidth, LV_SIZE_CONTENT);
    _low_note_label->setPos(kWhiteKeyX.front(), kNoteLabelY);
    _low_note_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _low_note_label->setTextFont(uiFont14());
    _low_note_label->setTextColor(lv_color_hex(kNoteTextColor));

    _high_note_label = std::make_unique<Label>(_root->raw_ptr());
    _high_note_label->setSize(kWhiteKeyWidth, LV_SIZE_CONTENT);
    _high_note_label->setPos(kWhiteKeyX.back(), kNoteLabelY);
    _high_note_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _high_note_label->setTextFont(uiFont14());
    _high_note_label->setTextColor(lv_color_hex(kNoteTextColor));

    _header = std::make_unique<Container>(_root->raw_ptr());
    _header->setSize(kScreenWidth, 22);
    _header->setPos(0, 0);
    setupContainer(*_header);
    _header->setBgColor(lv_color_hex(kHeaderColor));

    _header_divider = std::make_unique<Container>(_header->raw_ptr());
    _header_divider->setSize(kScreenWidth, 2);
    _header_divider->setPos(0, 20);
    setupContainer(*_header_divider);
    _header_divider->setBgColor(lv_color_hex(kHeaderDividerColor));

    _keymap_hint = std::make_unique<Label>(_header->raw_ptr());
    _keymap_hint->setSize(86, LV_SIZE_CONTENT);
    _keymap_hint->setPos(11, 4);
    _keymap_hint->setTextAlign(LV_TEXT_ALIGN_LEFT);
    _keymap_hint->setTextFont(uiFont10());
    _keymap_hint->setTextColor(lv_color_hex(kHeaderTextColor));
    _keymap_hint->setText("SPACE: Keymap");

    _mode_hint = std::make_unique<Label>(_header->raw_ptr());
    _mode_hint->setSize(58, LV_SIZE_CONTENT);
    _mode_hint->setPos(251, 4);
    _mode_hint->setTextAlign(LV_TEXT_ALIGN_RIGHT);
    _mode_hint->setTextFont(uiFont10());
    _mode_hint->setTextColor(lv_color_hex(kHeaderTextColor));
    _mode_hint->setText("TAB: Mode");

    _chord_panel = std::make_unique<Container>(_root->raw_ptr());
    _chord_panel->setSize(kChordPanelWidth, kChordPanelHeight);
    _chord_panel->setPos(kChordPanelX, kChordPanelY);
    setupContainer(*_chord_panel);
    _chord_panel->setBgColor(lv_color_hex(kChordPanelColor));
    _chord_panel->setBorderWidth(1);
    _chord_panel->setBorderColor(lv_color_hex(kChordPanelBorderColor));
    _chord_panel->setRadius(kChordPanelRadius);
    _chord_panel->setHidden(true);

    _chord_key_label = std::make_unique<Label>(_chord_panel->raw_ptr());
    _chord_key_label->setSize(kChordPanelWidth - 16, LV_SIZE_CONTENT);
    _chord_key_label->setPos(8, 3);
    _chord_key_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _chord_key_label->setTextFont(uiFont14());
    _chord_key_label->setTextColor(lv_color_hex(kChordTitleColor));

    _active_chord_label = std::make_unique<Label>(_chord_panel->raw_ptr());
    _active_chord_label->setSize(kChordPanelWidth - 16, LV_SIZE_CONTENT);
    _active_chord_label->setPos(8, 21);
    _active_chord_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _active_chord_label->setTextFont(uiFont10());
    _active_chord_label->setTextColor(lv_color_hex(kChordDetailColor));

    _playalong_keymap_panel = std::make_unique<Container>(_root->raw_ptr());
    _playalong_keymap_panel->setSize(kPlayalongPanelWidth, kPlayalongPanelHeight);
    _playalong_keymap_panel->setPos(kPlayalongPanelX, kPlayalongPanelY);
    setupContainer(*_playalong_keymap_panel);
    _playalong_keymap_panel->setBgColor(lv_color_hex(kBlackBadgeColor));
    _playalong_keymap_panel->setBorderWidth(1);
    _playalong_keymap_panel->setBorderColor(lv_color_hex(kBlackBadgeBorderColor));
    _playalong_keymap_panel->setRadius(kPlayalongPanelRadius);
    _playalong_keymap_panel->setHidden(true);
    _playalong_keymap_panel_visible = false;

    _playalong_keymap_badge = std::make_unique<Container>(_playalong_keymap_panel->raw_ptr());
    _playalong_keymap_badge->setSize(kMapBadgeWidth, kMapBadgeHeight);
    _playalong_keymap_badge->setPos(kPlayalongBadgeX, kPlayalongBadgeY);
    setupContainer(*_playalong_keymap_badge);
    _playalong_keymap_badge->setBgColor(lv_color_hex(kWhiteBadgeColor));
    _playalong_keymap_badge->setBorderWidth(1);
    _playalong_keymap_badge->setBorderColor(lv_color_hex(kWhiteBadgeBorderColor));
    _playalong_keymap_badge->setRadius(kMapBadgeRadius);

    _playalong_keymap_badge_label = std::make_unique<Label>(_playalong_keymap_badge->raw_ptr());
    _playalong_keymap_badge_label->setSize(kMapBadgeWidth, LV_SIZE_CONTENT);
    _playalong_keymap_badge_label->align(LV_ALIGN_CENTER, 0, 0);
    _playalong_keymap_badge_label->setTextAlign(LV_TEXT_ALIGN_CENTER);
    _playalong_keymap_badge_label->setTextFont(uiFont14());
    _playalong_keymap_badge_label->setTextColor(lv_color_hex(kWhiteBadgeTextColor));
    _playalong_keymap_badge_label->setText("P");

    _playalong_keymap_label = std::make_unique<Label>(_playalong_keymap_panel->raw_ptr());
    _playalong_keymap_label->setSize(kPlayalongPanelWidth - kPlayalongLabelX, LV_SIZE_CONTENT);
    _playalong_keymap_label->setPos(kPlayalongLabelX, kPlayalongLabelY);
    _playalong_keymap_label->setTextAlign(LV_TEXT_ALIGN_LEFT);
    _playalong_keymap_label->setTextFont(uiFont10());
    _playalong_keymap_label->setTextColor(lv_color_hex(kBlackBadgeTextColor));
    _playalong_keymap_label->setText("Play Along!");

    lv_obj_move_foreground(_header->raw_ptr());

    _left_arrow = std::make_unique<Canvas>(_root->raw_ptr());
    setupCanvas(*_left_arrow);
    _left_arrow->createBuffer(13, 22);
    _left_arrow->setPos(0, 80);
    _left_arrow->fillBg(lv_color_hex(kBackgroundColor));
    _left_arrow->startDrawing();
    _left_arrow->drawTriangle(std::vector<lv_point_t>{{3, 11}, {12, 0}, {12, 21}}, lv_color_hex(kArrowColor));
    _left_arrow->finishDrawing();

    _right_arrow = std::make_unique<Canvas>(_root->raw_ptr());
    setupCanvas(*_right_arrow);
    _right_arrow->createBuffer(13, 22);
    _right_arrow->setPos(307, 80);
    _right_arrow->fillBg(lv_color_hex(kBackgroundColor));
    _right_arrow->startDrawing();
    _right_arrow->drawTriangle(std::vector<lv_point_t>{{1, 0}, {10, 11}, {1, 21}}, lv_color_hex(kArrowColor));
    _right_arrow->finishDrawing();

    _playalong_effects                = std::make_unique<PlayalongEffects>(_root->raw_ptr());
    _shown_playalong_success_revision = _view_model.state().get().playalong_success_revision;

    _view_model.state().observe(this, onStateChanged);
}

void PianoView::onExit()
{
    _view_model.state().removeObserver();
    if (_playalong_effects) {
        _playalong_effects->stop();
    }
    _playalong_effects.reset();
    _right_arrow.reset();
    _left_arrow.reset();
    _playalong_keymap_label.reset();
    _playalong_keymap_badge_label.reset();
    _playalong_keymap_badge.reset();
    _playalong_keymap_panel.reset();
    _active_chord_label.reset();
    _chord_key_label.reset();
    _chord_panel.reset();
    _mode_hint.reset();
    _keymap_hint.reset();
    _keymap_visuals_overridden        = false;
    _keymap_hint_highlighted          = false;
    _playalong_keymap_panel_visible   = false;
    _playalong_label_active           = false;
    _shown_playalong_demo_target      = -1;
    _shown_playalong_guide_target     = -1;
    _shown_playalong_success_revision = 0;
    _header_divider.reset();
    _header.reset();
    _high_note_label.reset();
    _low_note_label.reset();
    for (auto& key : _black_keys) {
        key.reset();
    }
    for (auto& key : _white_keys) {
        key.reset();
    }
    _root.reset();
}

void PianoView::tick(uint32_t now_ms)
{
    if (_playalong_effects) {
        _playalong_effects->tick(now_ms);
    }

    const PianoState& state    = _view_model.state().get();
    const int guide_target     = state.playalong_phase == PlayalongPhase::Guide ? state.playalong_target : -1;
    const int demo_target      = state.playalong_phase == PlayalongPhase::Demo ? state.playalong_demo_target : -1;
    const int success_target   = _playalong_effects ? _playalong_effects->celebratingTarget() : -1;
    const int target_lift      = _playalong_effects ? _playalong_effects->targetLift() : 0;
    const int demo_lift        = _playalong_effects ? _playalong_effects->demoBeatLift() : 0;
    const int success_lift     = _playalong_effects ? _playalong_effects->successLift() : 0;
    const int success_scale    = _playalong_effects ? _playalong_effects->successScale() : 256;
    const bool effects_visible = guide_target >= 0 || demo_target >= 0 || success_target >= 0;
    if (effects_visible || _keymap_visuals_overridden) {
        for (std::size_t index = 0; index < _white_keys.size(); ++index) {
            const bool is_guide_target     = static_cast<int>(index) == guide_target;
            const bool is_demo_target      = static_cast<int>(index) == demo_target;
            const bool celebrating         = static_cast<int>(index) == success_target;
            const bool visible             = _keymap_visible || is_guide_target || is_demo_target || celebrating;
            const BadgeHighlight highlight = (is_guide_target || celebrating)
                                                 ? BadgeHighlight::Guide
                                                 : (is_demo_target ? BadgeHighlight::Demo : BadgeHighlight::None);
            _white_keys[index]->applyKeymapVisual(visible, LV_OPA_COVER, highlight,
                                                  (is_guide_target ? target_lift : 0) +
                                                      (is_demo_target ? demo_lift : 0) +
                                                      (celebrating ? success_lift : 0),
                                                  celebrating ? success_scale : 256);
        }
        for (auto& key : _black_keys) {
            key->applyKeymapVisual(_keymap_visible, LV_OPA_COVER, BadgeHighlight::None, 0, 256);
        }
    }
    _keymap_visuals_overridden = effects_visible;
}

void PianoView::render(const PianoState& state)
{
    const bool chord_mode = state.mode == PianoMode::Chord;

    const int guide_target = state.playalong_phase == PlayalongPhase::Guide ? state.playalong_target : -1;
    if (_playalong_effects && guide_target != _shown_playalong_guide_target) {
        _shown_playalong_guide_target = guide_target;
        if (guide_target >= 0 && guide_target < static_cast<int>(_white_keys.size())) {
            _playalong_effects->startGuideBob();
        } else {
            _playalong_effects->cancelGuideBob();
        }
    }

    const int demo_target = state.playalong_phase == PlayalongPhase::Demo ? state.playalong_demo_target : -1;
    if (_playalong_effects && demo_target != _shown_playalong_demo_target) {
        _shown_playalong_demo_target = demo_target;
        if (demo_target >= 0 && demo_target < static_cast<int>(_white_keys.size())) {
            _playalong_effects->pulseDemoBeat();
        } else {
            _playalong_effects->cancelDemoBeat();
        }
    }

    const bool new_playalong_success = state.playalong_success_revision != _shown_playalong_success_revision;
    if (_playalong_effects && new_playalong_success) {
        _shown_playalong_success_revision = state.playalong_success_revision;
        if (state.playalong_success_target >= 0 &&
            state.playalong_success_target < static_cast<int>(_white_keys.size())) {
            const auto& key = _white_keys[static_cast<std::size_t>(state.playalong_success_target)];
            _playalong_effects->celebrate(state.playalong_success_target, key->badgeCenterX(), key->badgeCenterY());
        }
    } else if (_playalong_effects && state.playalong_success_target < 0) {
        _playalong_effects->cancelCelebration();
    }

    for (auto& key : _white_keys) {
        key->render(state);
    }
    for (auto& key : _black_keys) {
        key->render(state);
    }

    _low_note_label->setHidden(chord_mode);
    _high_note_label->setHidden(chord_mode);
    if (!chord_mode) {
        _low_note_label->setText("C" + std::to_string(state.octave));
        _high_note_label->setText("C" + std::to_string(state.octave + 1));
    }
    _keymap_visible = state.keymap_visible;
    setKeymapHintHighlighted(state.keymap_visible);
    setPlayalongKeymapPanelVisible(state.keymap_visible);
    setPlayalongLabelActive(state.playalong_active);

    _chord_panel->setHidden(!chord_mode);
    if (chord_mode) {
        _chord_panel->setBgColor(
            lv_color_hex(state.tonality_toggle_pressed ? kChordPanelPressedColor : kChordPanelColor));
        _chord_key_label->setText(keyName(state.tonic, state.tonality) +
                                  (state.tonality == ChordTonality::Major ? " Major" : " Minor"));

        if (state.active_chord_index >= 0) {
            const auto chord_index = static_cast<std::size_t>(state.active_chord_index);
            _active_chord_label->setText(chordLabel(state.tonic, state.tonality, chord_index) + " / " +
                                         chordDegreeLabel(state.tonality, chord_index));
            _active_chord_label->setTextColor(lv_color_hex(kChordActiveColor));
        } else {
            _active_chord_label->setText("M: Major / Minor");
            _active_chord_label->setTextColor(lv_color_hex(kChordDetailColor));
        }
    }

    _left_arrow->setOpa(state.octave_down_pressed ? kArrowPressedOpacity : kArrowReleasedOpacity);
    _left_arrow->setX(state.octave_down_pressed ? 1 : 0);
    _right_arrow->setOpa(state.octave_up_pressed ? kArrowPressedOpacity : kArrowReleasedOpacity);
    _right_arrow->setX(state.octave_up_pressed ? 306 : 307);
}

void PianoView::setKeymapHintHighlighted(bool highlighted)
{
    if (!_keymap_hint || _keymap_hint_highlighted == highlighted) {
        return;
    }
    _keymap_hint->setTextColor(lv_color_hex(highlighted ? kKeymapHintColor : kHeaderTextColor));
    _keymap_hint_highlighted = highlighted;
}

void PianoView::setPlayalongKeymapPanelVisible(bool visible)
{
    if (!_playalong_keymap_panel || _playalong_keymap_panel_visible == visible) {
        return;
    }
    _playalong_keymap_panel->setHidden(!visible);
    _playalong_keymap_panel_visible = visible;
}

void PianoView::setPlayalongLabelActive(bool active)
{
    if (!_playalong_keymap_label || _playalong_label_active == active) {
        return;
    }
    _playalong_keymap_label->setTextColor(lv_color_hex(active ? kPlayalongActiveColor : kBlackBadgeTextColor));
    _playalong_label_active = active;
}

void PianoView::onStateChanged(void* context, const PianoState& state)
{
    auto* view = static_cast<PianoView*>(context);
    if (view) {
        view->render(state);
    }
}

}  // namespace piano
