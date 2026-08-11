#include "assets/font_assets.hpp"

extern "C" {
LV_FONT_DECLARE(font_noto_sans_sc_semibold_10);
LV_FONT_DECLARE(font_noto_sans_sc_semibold_14);
}

namespace piano {
namespace {

bool g_initialized = false;
lv_font_t g_ui_10;
lv_font_t g_ui_14;

}  // namespace

void initFontAssets()
{
    if (g_initialized) {
        return;
    }

    g_ui_10          = font_noto_sans_sc_semibold_10;
    g_ui_14          = font_noto_sans_sc_semibold_14;
    g_ui_10.fallback = nullptr;
    g_ui_14.fallback = nullptr;
    g_initialized    = true;
}

const lv_font_t* uiFont10()
{
    initFontAssets();
    return &g_ui_10;
}

const lv_font_t* uiFont14()
{
    initFontAssets();
    return &g_ui_14;
}

}  // namespace piano
