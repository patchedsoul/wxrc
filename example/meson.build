wayland_egl = dependency('wayland-egl')
libdrm = dependency('libdrm').partial_dependency(compile_args: true, includes: true)

client_deps = [
	cglm,
	egl,
	gbm,
	glesv2,
	libdrm,
	wayland_client,
	wayland_egl,
	wxrc_protocols,
]

executable('cube',
	files('cube.c'),
	dependencies: client_deps,
	install: true,
)

executable('fractal',
	files('fractal.c'),
	dependencies: client_deps,
	install: true,
)
