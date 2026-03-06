project('yis', 'c',
  version : '0.1.0',
  default_options : ['warning_level=2', 'c_std=c11']
)

yis_c_sources = files(
  'main.c',
  'lexer.c',
  'parser.c',
  'project.c',
  'sum_validate.c',
  'codegen.c',
  'typecheck.c',
  'file.c',
  'platform.c',
  'arena.c',
  'str.c',
  'diag.c',
  'external_module.c',
)

yis_c_includes = include_directories('.')

yis_bootstrap = executable(
  'yis-bootstrap',
  yis_c_sources,
  include_directories : yis_c_includes,
  install : false
)

yis_target = custom_target(
  'yis',
  input : files('../yi_src/init.yi', '../yi_src/lexer.yi', '../yi_src/parser.yi'),
  output : 'yis',
  command : [
    'sh',
    '-c',
    'cd "$3" && rm -f .yi_out .yi_compile_out.c "$2" && "$1" run yi_src/init.yi -- yi_src/init.yi && mv .yi_out "$2" && rm -f .yi_out .yi_compile_out.c',
    'sh',
    yis_bootstrap,
    meson.current_build_dir() / 'yis',
    meson.project_source_root() / '..'
  ],
  build_by_default : true,
  depends : yis_bootstrap,
  install : true,
  install_dir : get_option('bindir')
)

# Install stdlib files
install_data(
  'stdlib/stdr.yi',
  'stdlib/math.yi',
  install_dir : get_option('datadir') / 'yis' / 'stdlib'
)