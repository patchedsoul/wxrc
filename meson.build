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

	'-DXR_USE_GRAPHICS_API_OPENGL_ES',
	'-DXR_USE_PLATFORM_EGL',

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

cglm = dependency('cglm')
egl = dependency('egl')
gbm = dependency('gbm')
glesv2 = dependency('glesv2')
openxr = dependency('openxr')
xkbcommon = dependency('xkbcommon')
wayland_client = dependency('wayland-client')
wayland_server = dependency('wayland-server')
wayland_protocols = dependency('wayland-protocols')

# Try first to find wlroots as a subproject, then as a system dependency
wlroots_version = ['>=0.8.1', '<0.9.0']
wlroots_proj = subproject(
	'wlroots',
	default_options: ['examples=false'],
	required: false,
	version: wlroots_version,
)
if wlroots_proj.found()
	wlroots = wlroots_proj.get_variable('wlroots')
else
	wlroots = dependency('wlroots', version: wlroots_version)
endif

subdir('protocol')

wxrc_inc = include_directories('include')

executable('wxrc',
	files(
		'src/backend.c',
		'src/input.c',
		'src/main.c',
		'src/mathutil.c',
		'src/render.c',
		'src/view.c',
		'src/xdg-shell.c',
		'src/xr-shell-protocol.c',
		'src/xr-shell.c',
		'src/xrutil.c',
	),
	dependencies: [
		cglm,
		egl,
		glesv2,
		openxr,
		wlroots,
		xkbcommon,
		wayland_client,
		wayland_server,
		wxrc_protocols,
	],
	include_directories: wxrc_inc,
	install: true,
)

subdir('example')
