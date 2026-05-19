# svg2raylib

`svg2raylib.py` converts simple single-color SVG icons into raylib C++ drawing
functions. It is intended for UI icons and small vector ornaments that should
inherit theme colors without shipping extra texture assets.

## Usage

```powershell
python tools/svg2raylib.py icons/play.svg icons/save.svg `
  --out src/generated/icons.cpp `
  --header-out src/generated/icons.h `
  --include generated/icons.h `
  --namespace app_icons
```

You can also generate from a manifest:

```json
{
  "icons": [
    {
      "name": "settings_gear",
      "source": "https://raw.githubusercontent.com/lucide-icons/lucide/main/icons/settings.svg"
    }
  ]
}
```

```powershell
python tools/svg2raylib.py `
  --manifest assets/icons/raythm_icons.json `
  --out build/generated/ui/icons/raythm_icons.cpp `
  --header-out build/generated/ui/icons/raythm_icons.h `
  --include ui/icons/raythm_icons.h `
  --namespace raythm_icons `
  --sdf-atlas
```

Inputs can also be HTTP(S) URLs, which is handy for quickly checking icon sets
without downloading files by hand:

```powershell
python tools/svg2raylib.py `
  https://raw.githubusercontent.com/lucide-icons/lucide/main/icons/play.svg `
  --out build/icons.cpp `
  --header-out build/icons.h `
  --include icons.h `
  --preview-main build/svg2raylib_preview.cpp
```

The generated API looks like this:

```cpp
namespace app_icons {
void draw_play(Rectangle bounds, Color color, float stroke_width = 2.0f);
void draw_save(Rectangle bounds, Color color, float stroke_width = 2.0f);
}
```

## Supported SVG

- `path`: `M`, `L`, `H`, `V`, `C`, `Q`, `A`, `Z` and relative variants
- `line`
- `rect`, including `rx`/`ry` rounded corners
- `circle`
- `polyline`
- `polygon`
- `viewBox` scaling into the supplied raylib `Rectangle`

Unsupported SVG features such as transforms, gradients, masks, filters, text,
clip paths, and rounded rectangles are reported as warnings. The converter is
deliberately strict enough to make missing visual support visible during asset
generation instead of silently pretending to render full SVG.

## Notes

- Curves and circles are approximated as `DrawLineEx` polylines.
- The output is single-color: pass the desired `Color` at draw time.
- `--sdf-atlas` embeds a generated SDF atlas in the `.cpp` and renders it with
  a small shader, which keeps small UI icons smoother than direct line drawing.
- `--preview-main` emits a small raylib preview program that draws every
  generated icon in a grid.
- raythm's CMake build uses `assets/icons/raythm_icons.json` to generate
  `raythm_icons.cpp/h` into the build directory automatically.
- Complex illustration SVGs should still be rasterized or hand-authored. This
  tool is for compact icon geometry.
