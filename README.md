# wxrc

Wayland XR Compositor

This is a Wayland compositor which displays windows in a 3D virtual reality
scene, based on OpenXR and wlroots.

Status: reasonably usable as a daily driver for 2D content; 3D content is at a
proof-of-concept level of completion.

## Usage

In order to complete this, we had to patch large swaths of the broader
ecosystem. To get it working, you may have to find and apply our patches for at
least:

- Mesa
- Monado
- OpenXR
- Sway
- Vulkan
- Xwayland
- wlroots

This is left as an exercise to the reader. Best of luck.

## Video

https://spacepub.space/videos/watch/f60bee0e-31d3-4aca-9e49-6fcdc87ad40d
