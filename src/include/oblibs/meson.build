fs = import('fs')

conf = configuration_data()
conf.set_quoted('VERSION', meson.project_version())

configure_file(
  input: 'config.h.in',
  output: 'config.h',
  configuration: conf,
  install: true,
  install_dir: INCLUDEDIR
)