project('yis', 'c',
  version : '1.0.0',
  default_options : ['warning_level=2', 'c_std=c11', 'cpp_std=c++17']
)

if host_machine.system() != 'windows'
  # The bootstrap uses POSIX APIs that strict C11 hides by default.
  add_project_arguments('-D_POSIX_C_SOURCE=200809L', '-D_XOPEN_SOURCE=700', language : 'c')
endif

if host_machine.system() == 'darwin'
  add_project_arguments('-D_DARWIN_C_SOURCE', language : 'c')
endif

yis_c_sources = files(
  'src/bootstrap/main.c',
  'src/bootstrap/lexer.c',
  'src/bootstrap/parser.c',
  'src/bootstrap/project.c',
  'src/bootstrap/sum_validate.c',
  'src/bootstrap/codegen.c',
  'src/bootstrap/typecheck.c',
  'src/bootstrap/file.c',
  'src/bootstrap/platform.c',
  'src/bootstrap/arena.c',
  'src/bootstrap/str.c',
  'src/bootstrap/diag.c',
  'src/bootstrap/external_module.c',
)

yis_c_includes = include_directories('src', 'src/bootstrap')

yis_bootstrap = executable(
  'yis-bootstrap',
  yis_c_sources,
  include_directories : yis_c_includes,
  install : false
)

yis_target = custom_target(
  'yis',
  input : files('src/init.yi', 'src/lexer.yi', 'src/parser.yi'),
  output : 'yis',
  command : [
    'sh',
    '-c',
    'ROOT="$3" && rm -f "$ROOT/.yi_out" "$ROOT/.yi_compile_out.c" "$ROOT/.yi_run_out" "$ROOT/.yi_run_compile_out.c" "$2" && cd "$ROOT" && YIS_STDLIB="$ROOT/src/stdlib" "$1" run "$ROOT/src/init.yi" -- "$ROOT/src/init.yi" && mv "$ROOT/.yi_out" "$2" && rm -f "$ROOT/.yi_out" "$ROOT/.yi_compile_out.c" "$ROOT/.yi_run_out" "$ROOT/.yi_run_compile_out.c"',
    'sh',
    yis_bootstrap,
    meson.current_build_dir() / 'yis',
    meson.project_source_root()
  ],
  build_by_default : true,
  depends : yis_bootstrap,
  depend_files : files('src/runtime.inc', 'src/stdlib/stdr.yi'),
  install : true,
  install_dir : get_option('bindir')
)

test_python = find_program('python3', required : false)
if test_python.found()
  test('bootstrap-regressions', test_python,
    args : [files('tests/test_bootstrap.py'), yis_bootstrap, meson.project_source_root()],
    timeout : 120
  )
  foreach suite : [
    ['compiler-regressions', 'tests/test_compiler.py'],
    ['driver-configuration', 'tests/test_driver_config.py'],
    ['optimizer-regressions', 'tests/test_optimizer.py'],
    ['codegen-ownership', 'tests/test_codegen_memory.py'],
    ['parser-regressions', 'tests/test_parser.py'],
    ['string-regressions', 'tests/test_strings.py'],
  ]
    test(suite[0], test_python,
      args : [files(suite[1]), yis_target, meson.project_source_root()],
      timeout : 180
    )
  endforeach
  test('native-runtime', test_python, args : files('tests/test_runtime.py'), timeout : 120)
  if host_machine.system() == 'linux'
    test('native-runtime-sanitizers', test_python,
      args : [files('tests/test_runtime.py'), '--sanitizers'], timeout : 120
    )
  endif
endif

install_data(
  'src/runtime.inc',
  install_dir : get_option('datadir') / 'yis'
)

install_data(
  'src/stdlib/stdr.yi',
  'src/stdlib/math.yi',
  'src/stdlib/net.yi',
  'src/stdlib/json.yi',
  'src/stdlib/base64.yi',
  'src/stdlib/datetime.yi',
  'src/stdlib/regex.yi',
  'src/stdlib/ffmpeg.yi',
  'src/stdlib/poppler.yi',
  install_dir : get_option('datadir') / 'yis' / 'stdlib'
)
