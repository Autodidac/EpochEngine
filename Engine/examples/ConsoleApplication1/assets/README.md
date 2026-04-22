# Example Asset Layout

This folder holds the tracked source images used by the example runtime.

## Source-of-truth folders

- `defaults/` - small fallback textures and simple baseline images.
- `fonts/` - tracked UI/runtime fonts that ship with the example.
- `games/` - per-example game sprites and textures.
- `menu/` - the current menu/button image source used for tracked menu art.
- `vulkan/` - example-local Vulkan textures and shader inputs.

## Archive folders

- `archive/legacy_menu_reference/` - older menu-atlas source images kept only
  as historical reference.
- `archive/unused_reference_images/` - unused older reference images kept for
  comparison, not for active runtime loading.

## What does not belong here

- Prebaked atlas outputs do not live in this folder. They belong in
  `../atlases/`.
- Disposable atlas debug dumps do not live in this folder. They belong in the
  ignored local dump paths beside the example runtime.
