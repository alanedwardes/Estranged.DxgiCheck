# Estranged.DxgiCheck

A small Unreal Engine plugin that checks for required DirectX 12 hardware features before the RHI initialises. If the player's GPU doesn't support what your game needs, they get a clear error message instead of a crash or black screen.

## How it works

The plugin loads at `EarliestPossible` phase, before the rendering system starts. It reads your project's renderer settings to figure out what features you've enabled (Nanite, Virtual Shadow Maps, Hardware Ray Tracing), then queries the GPU to see if it can actually deliver.

If something's missing, the player sees a message box listing exactly what their hardware lacks, with an option to open a help page.

## Supported feature checks

| Feature | Config Key | Required Hardware |
|---------|-----------|-------------------|
| Nanite | `r.Nanite` | 64-bit Atomics, Wave Ops, SM6.6 |
| Virtual Shadow Maps | `r.Shadow.Virtual.Enable` | SM6.6 |
| Hardware Ray Tracing | `r.RayTracing` | DXR Tier 1.1 |

## Configuration

Add to your `DefaultEngine.ini` to customise the error dialog:

```ini
[/Script/EstDxgiStats.EstDxgiCheck]
ErrorMessage=Your graphics card doesn't support the required features for this game.
ErrorTitle=Incompatible Hardware
HelpURL=https://yourgame.com/system-requirements
```

The help URL receives query parameters indicating which features are missing (e.g. `?missing=atomic64&missing=sm66`), so you can show relevant troubleshooting info.

## Platform support

Windows only. The plugin won't load on other platforms.
