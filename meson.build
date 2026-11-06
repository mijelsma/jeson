project(
  'jeson',
  'cpp',
  version: '0.2.0',
  license: 'MIT',
  default_options: ['cpp_std=c++20'],
)

jeson_inc = include_directories('include')
jeson_dep = declare_dependency(include_directories: jeson_inc)
meson.override_dependency('jeson', jeson_dep)

install_subdir('include/jeson', install_dir: get_option('includedir'))
