#pragma once

class settings_runtime_applier {
public:
    void apply_bgm_volume(float volume) const;
    void apply_se_volume(float volume) const;
    void apply_fullscreen(bool fullscreen) const;
    void apply_theme(bool dark_mode) const;
};
