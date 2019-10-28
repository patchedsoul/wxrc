project(
	'wxrc',
	['c'],
	version: '0.1.0',
	license: 'MIT',
	meson_version: '>=0.49.0',
	default_options: [
		'c_std=c11',
		'warning_level=2',
	],
)

cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments([
	'-DWLR_USE_UNSTABLE',

	'-DXR_USE_GRAPHICS_API_OPENGL',
	'-DXR_USE_PLATFORM_WAYLAND',

	'-Wundef',
	'-Wlogical-op',
	'-Wmissing-include-dirs',
	'-Wold-style-definition',
	'-Wpointer-arith',
	'-Winit-self',
	'-Wstrict-prototypes',
	'-Wimplicit-fallthrough=2',
	'-Wendif-labels',
	'-Wstrict-aliasing=2',
	'-Woverflow',

	'-Wno-missing-braces',
	'-Wno-missing-field-initializers',
	'-Wno-unused-parameter',
]), language: 'c')

openxr  = dependency('openxr')
wlroots = dependency('wlroots')

wayland_client = dependency('wayland-client')

wxrc_inc = include_directories('include')

executable('wxrc',
	files(
		'src/main.c',
		'src/xrutil.c',
	),
	dependencies: [
		openxr,
		wlroots,
		wayland_client,
	],
	include_directories: wxrc_inc,
	install: true,
)
