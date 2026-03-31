#pragma once

#include "raylib.h"

namespace ui {

void push_clip_rect(Rectangle rect);
void pop_clip_rect();
bool has_clip_rect();
// クリップが積まれていない場合は空矩形を返す。
Rectangle current_clip_rect();

class scoped_clip_rect {
public:
    explicit scoped_clip_rect(Rectangle rect);
    ~scoped_clip_rect();

    scoped_clip_rect(const scoped_clip_rect&) = delete;
    scoped_clip_rect& operator=(const scoped_clip_rect&) = delete;

    scoped_clip_rect(scoped_clip_rect&& other) noexcept;
    scoped_clip_rect& operator=(scoped_clip_rect&& other) noexcept;

private:
    bool active_ = false;
};

}  // namespace ui
