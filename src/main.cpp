#include "core/piano_app.hpp"
#include "hal/piano_lvgl_hal.hpp"

#include <core/hal/hal.hpp>
#include <lvgl.h>
#include <spdlog/spdlog.h>

#include <cstdio>
#include <unistd.h>

int main()
{
    constexpr int32_t kScreenWidth  = 320;
    constexpr int32_t kScreenHeight = 170;

    lv_init();
    if (!piano::initLvglHal(kScreenWidth, kScreenHeight)) {
        return 1;
    }

    lv_display_t* display = lv_display_get_default();
    if (!display) {
        std::fprintf(stderr, "Piano: failed to create LVGL display\n");
        return 1;
    }
    spdlog::info("Piano: display {}x{}", static_cast<int>(lv_display_get_horizontal_resolution(display)),
                 static_cast<int>(lv_display_get_vertical_resolution(display)));

    smooth_ui_toolkit::ui_hal::on_get_tick([]() { return lv_tick_get(); });
    smooth_ui_toolkit::ui_hal::on_delay([](uint32_t milliseconds) { usleep(milliseconds * 1000); });

    piano::PianoApp app;
    if (!app.start(lv_screen_active())) {
        piano::shutdownLvglHal();
        return 1;
    }

    lv_obj_invalidate(lv_screen_active());
    while (!app.quitRequested()) {
        lv_timer_handler();
        if (!lv_display_get_default()) {
            spdlog::info("Piano: display closed");
            break;
        }
        app.tick(lv_tick_get());
        usleep(8000);
    }

    spdlog::info("Piano: exit requested");
    app.stop();
    piano::shutdownLvglHal();
    return 0;
}
