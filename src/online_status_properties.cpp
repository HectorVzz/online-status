// SPDX-License-Identifier: GPL-2.0-or-later
#include "online_status.hpp"

// Property UI visibility refresher (C-callable for OBS callbacks)
static bool online_status_properties_refresh(obs_properties_t *props, obs_property_t * /*property*/, obs_data_t *settings)
{
    auto set_vis = [&](const char *name, bool v) {
        if (obs_property_t *pp = obs_properties_get(props, name))
            obs_property_set_visible(pp, v);
    };

    int section = (int)obs_data_get_int(settings, "ui_section");
    int mode_val = (int)obs_data_get_int(settings, "content_mode");

    // Dropping group inner properties
    if (obs_property_t *grp = obs_properties_get(props, "dropping_group")) {
        obs_properties_t *inner = obs_property_group_content(grp);
        if (inner) {
            bool show_drop = (section == 0);
            if (obs_property_t *pp = obs_properties_get(inner, "content_mode"))
                obs_property_set_visible(pp, show_drop);
            if (obs_property_t *pp = obs_properties_get(inner, "status_text"))
                obs_property_set_visible(pp, show_drop && mode_val == 0);
            if (obs_property_t *pp = obs_properties_get(inner, "image_path"))
                obs_property_set_visible(pp, show_drop && mode_val == 1);
            if (obs_property_t *pp = obs_properties_get(inner, "drop_threshold_pct"))
                obs_property_set_visible(pp, show_drop);
            if (obs_property_t *pp = obs_properties_get(inner, "hide_after_sec"))
                obs_property_set_visible(pp, show_drop);
            if (obs_property_t *pp = obs_properties_get(inner, "drop_blink_enabled"))
                obs_property_set_visible(pp, show_drop);
            if (obs_property_t *pp = obs_properties_get(inner, "drop_blink_rate_hz"))
                obs_property_set_visible(pp, show_drop);
        }
    }

    // Stable group inner properties
    if (obs_property_t *grp = obs_properties_get(props, "stable_group")) {
        obs_properties_t *inner = obs_property_group_content(grp);
        if (inner) {
            bool s_enabled_v = obs_data_get_bool(settings, "stable_enabled");
            int s_mode = (int)obs_data_get_int(settings, "stable_mode");
            bool show_inner = (section == 1) && s_enabled_v;

            if (obs_property_t *pp = obs_properties_get(inner, "stable_mode"))
                obs_property_set_visible(pp, section == 1);
            if (obs_property_t *pp = obs_properties_get(inner, "stable_text"))
                obs_property_set_visible(pp, show_inner && s_mode == 0);
            if (obs_property_t *pp = obs_properties_get(inner, "stable_image_path"))
                obs_property_set_visible(pp, show_inner && s_mode == 1);
            if (obs_property_t *pp = obs_properties_get(inner, "stable_enabled"))
                obs_property_set_visible(pp, section == 1);
            if (obs_property_t *pp = obs_properties_get(inner, "stable_duration_sec"))
                obs_property_set_visible(pp, (section == 1) && s_enabled_v);
            if (obs_property_t *pp = obs_properties_get(inner, "stable_blink_enabled"))
                obs_property_set_visible(pp, (section == 1) && s_enabled_v);
            if (obs_property_t *pp = obs_properties_get(inner, "stable_blink_rate_hz"))
                obs_property_set_visible(pp, (section == 1) && s_enabled_v);
        }
    }

    // Pseudo-tabs visibility (no separate Blink tab)
    bool show_dropping = (section == 0);
    bool show_stable   = (section == 1);
    bool show_adv      = (section == 2);

    auto show_adv_field = [&](const char *name) { set_vis(name, show_adv); };
    show_adv_field("visible");
    show_adv_field("test_force_drop");
    show_adv_field("test_simulate_spike");
    show_adv_field("test_show_stable");
    show_adv_field("test_hide_all");

    if (obs_property_t *grp = obs_properties_get(props, "dropping_group"))
        obs_property_set_visible(grp, show_dropping);
    if (obs_property_t *grp = obs_properties_get(props, "stable_group"))
        obs_property_set_visible(grp, show_stable);
    return true;
}

// Advanced testing buttons
static bool online_status_btn_simulate_spike(obs_properties_t * /*props*/, obs_property_t * /*p*/, void *data)
{
    auto *s = static_cast<OnlineStatus *>(data);
    if (!s) return false;
    s->auto_visible = true;
    s->since_last_drop = 0.0f;
    s->stable_visible = false;
    s->stable_timer = 0.0f;
    return true; // refresh UI
}

static bool online_status_btn_show_stable(obs_properties_t * /*props*/, obs_property_t * /*p*/, void *data)
{
    auto *s = static_cast<OnlineStatus *>(data);
    if (!s) return false;
    s->auto_visible = false;
    s->stable_visible = true;
    s->stable_timer = s->stable_duration_sec;
    return true;
}

static bool online_status_btn_hide_all(obs_properties_t * /*props*/, obs_property_t * /*p*/, void *data)
{
    auto *s = static_cast<OnlineStatus *>(data);
    if (!s) return false;
    s->auto_visible = false;
    s->stable_visible = false;
    s->stable_timer = 0.0f;
    return true;
}

obs_properties_t *online_status_properties(void *data)
{
    UNUSED_PARAMETER(data);
    obs_properties_t *props = obs_properties_create();

    // Section selector to simulate tabs
    obs_property_t *section = obs_properties_add_list(
        props, "ui_section", "Section",
        OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(section, "Dropping", 0);
    obs_property_list_add_int(section, "Stable", 1);
    obs_property_list_add_int(section, "Advanced", 2);

    // Dropping overlay group
    obs_properties_t *dropping = obs_properties_create();
    obs_property_t *mode = obs_properties_add_list(dropping, "content_mode", "Content Type (when dropping)",
                                                   OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(mode, "Text", 0);
    obs_property_list_add_int(mode, "Image", 1);

    obs_properties_add_text(dropping, "status_text", "Text to show while dropping", OBS_TEXT_DEFAULT);
    obs_properties_add_path(dropping, "image_path", "Image file (while dropping)", OBS_PATH_FILE,
                            "Image files (*.png *.jpg *.jpeg *.bmp *.gif);;All files (*.*)", nullptr);

    obs_properties_add_float_slider(dropping, "drop_threshold_pct", "Drop % threshold (per-interval)", 0.0, 100.0,
        0.1);
    obs_properties_add_float_slider(dropping, "hide_after_sec", "Hide after seconds without drops", 0.0, 30.0, 0.1);
    // Blink controls (Dropping)
    obs_properties_add_bool(dropping, "drop_blink_enabled", "Blink (while dropping)");
    obs_properties_add_float_slider(dropping, "drop_blink_rate_hz", "Blink rate (Hz)", 0.0, 10.0, 0.1);

    obs_property_t *dropping_group = obs_properties_add_group(props, "dropping_group", "When dropping frames",
                                                             OBS_GROUP_NORMAL, dropping);
    obs_properties_add_bool(props, "visible", "Test text that auto-shows when dropping frames(for debugging)");

    // Stable overlay group
    obs_properties_t *stable = obs_properties_create();
    obs_properties_add_bool(stable, "stable_enabled", "Show message when connection is stable");
    obs_property_t *smode = obs_properties_add_list(stable, "stable_mode", "Content Type (when stable)",
                                                    OBS_COMBO_TYPE_LIST, OBS_COMBO_FORMAT_INT);
    obs_property_list_add_int(smode, "Text", 0);
    obs_property_list_add_int(smode, "Image", 1);
    obs_properties_add_text(stable, "stable_text", "Stable text", OBS_TEXT_DEFAULT);
    obs_properties_add_path(stable, "stable_image_path", "Stable image file", OBS_PATH_FILE,
                            "Image files (*.png *.jpg *.jpeg *.bmp *.gif);;All files (*.*)", nullptr);
    obs_properties_add_float_slider(stable, "stable_duration_sec", "Stable message duration (seconds)",
                                    0.0, 60.0, 0.1);
    // Blink controls (Stable)
    obs_properties_add_bool(stable, "stable_blink_enabled", "Blink (stable message)");
    obs_properties_add_float_slider(stable, "stable_blink_rate_hz", "Blink rate (Hz)", 0.0, 10.0, 0.1);

    obs_property_t *stable_group = obs_properties_add_group(props, "stable_group", "When connection stabilizes", OBS_GROUP_NORMAL, stable);

    // Advanced: testing controls
    obs_properties_add_bool(props, "test_force_drop", "Test: Force dropping overlay");
    obs_properties_add_button(props, "test_simulate_spike", "Test: Simulate drop spike", online_status_btn_simulate_spike);
    obs_properties_add_button(props, "test_show_stable", "Test: Show stable message now", online_status_btn_show_stable);
    obs_properties_add_button(props, "test_hide_all", "Test: Hide overlays", online_status_btn_hide_all);

    // Dynamic visibility handled by C-callback online_status_properties_refresh()

    // Hook callbacks
    obs_property_set_modified_callback(section, online_status_properties_refresh);
    obs_property_set_modified_callback(mode,    online_status_properties_refresh);
    // Hook inner dropping toggles
    if (obs_property_t *grp = dropping_group) {
        obs_properties_t *inner = obs_property_group_content(grp);
        if (inner) {
            if (obs_property_t *pp = obs_properties_get(inner, "content_mode"))
                obs_property_set_modified_callback(pp, online_status_properties_refresh);
        }
    }
    // Also attach to stable group inner toggles
    if (obs_property_t *grp = stable_group) {
        obs_properties_t *inner = obs_property_group_content(grp);
        if (inner) {
            if (obs_property_t *pp = obs_properties_get(inner, "stable_enabled"))
                obs_property_set_modified_callback(pp, online_status_properties_refresh);
            if (obs_property_t *pp = obs_properties_get(inner, "stable_mode"))
                obs_property_set_modified_callback(pp, online_status_properties_refresh);
        }
    }
    return props;
}
