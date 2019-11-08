wayland_egl = dependency('wayland-egl')

executable('cube',
	files('cube.c'),
	dependencies: [
		cglm,
		egl,
		glesv2,
		wayland_client,
		wayland_egl,
		wxrc_protocols,
	],
	include_directories: wxrc_inc,
	install: true,
)
