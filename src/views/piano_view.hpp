#pragma once

#include "core/piano_types.hpp"
#include "view_models/piano_view_model.hpp"

#include <lvgl.h>

#include <array>
#include <cstdint>
#include <memory>

namespace smooth_ui_toolkit::lvgl_cpp {
class Canvas;
class Container;
class Label;
}  // namespace smooth_ui_toolkit::lvgl_cpp

namespace piano {

class PianoView {
public:
    explicit PianoView(PianoViewModel& view_model);
    ~PianoView();

    void onEnter(lv_obj_t* parent);
    void onExit();
    void tick(uint32_t now_ms);

private:
    struct KeyVisual;
    struct PlayalongEffects;

    PianoViewModel& _view_model;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _root;
    std::array<std::unique_ptr<KeyVisual>, 8> _white_keys;
    std::array<std::unique_ptr<KeyVisual>, 5> _black_keys;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _low_note_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _high_note_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _header;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _header_divider;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _keymap_hint;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _mode_hint;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _chord_panel;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _chord_key_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _active_chord_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _playalong_keymap_panel;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Container> _playalong_keymap_badge;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _playalong_keymap_badge_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Label> _playalong_keymap_label;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Canvas> _left_arrow;
    std::unique_ptr<smooth_ui_toolkit::lvgl_cpp::Canvas> _right_arrow;
    std::unique_ptr<PlayalongEffects> _playalong_effects;
    bool _keymap_visible                       = false;
    bool _keymap_visuals_overridden            = false;
    bool _keymap_hint_highlighted              = false;
    bool _playalong_keymap_panel_visible       = false;
    bool _playalong_label_active               = false;
    int _shown_playalong_demo_target           = -1;
    int _shown_playalong_guide_target          = -1;
    uint32_t _shown_playalong_success_revision = 0;

    void render(const PianoState& state);
    void setKeymapHintHighlighted(bool highlighted);
    void setPlayalongKeymapPanelVisible(bool visible);
    void setPlayalongLabelActive(bool active);
    static void onStateChanged(void* context, const PianoState& state);
};

}  // namespace piano
