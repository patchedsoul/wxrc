wl_protocols_dir = wayland_protocols.get_pkgconfig_variable('pkgdatadir')

wayland_scanner_dep = dependency('wayland-scanner', native: true)
wayland_scanner = find_program(
	wayland_scanner_dep.get_pkgconfig_variable('wayland_scanner'),
	native: true,
)

protocols = [
	[wl_protocols_dir, 'stable/xdg-shell/xdg-shell.xml'],
]

wl_protocols_src = []
wl_protocols_headers = []

foreach p : protocols
	xml = join_paths(p)
	wl_protocols_src += custom_target(
		xml.underscorify() + '_server_c',
		input: xml,
		output: '@BASENAME@-protocol.c',
		command: [wayland_scanner, 'private-code', '@INPUT@', '@OUTPUT@'],
	)
	wl_protocols_headers += custom_target(
		xml.underscorify() + '_server_h',
		input: xml,
		output: '@BASENAME@-protocol.h',
		command: [wayland_scanner, 'server-header', '@INPUT@', '@OUTPUT@'],
	)
endforeach

lib_wl_protocols = static_library(
	'wl_protocols',
	wl_protocols_src + wl_protocols_headers,
	dependencies: wayland_server.partial_dependency(compile_args: true),
)

wxrc_protocols = declare_dependency(
	link_with: lib_wl_protocols,
	sources: wl_protocols_headers,
)
