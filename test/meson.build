test_compiler_flags = compiler_flags

# The resources' absolute path, on the tests only: as a project argument it went into every
# library object too, so no two checkouts could share a compiler cache entry (js-seq).
test_compiler_flags += ['-DTEST_DIR="@0@/resources"'.format('/'.join(meson.current_source_dir().split('\\')))]

if lib_type == 'static'
    test_compiler_flags += ['-DTVG_STATIC']
endif

test_dep = []
if host_machine.system() == 'darwin'
    test_dep += declare_dependency(link_args: ['-framework', 'Cocoa', '-framework', 'IOKit'])
endif

test_file = [
    'testAccessor.cpp',
    'testAnimation.cpp',
    'testFill.cpp',
    'testInitializer.cpp',
    'testLottie.cpp',
    'testMain.cpp',
    'testPaint.cpp',
    'testPicture.cpp',
    'testSavers.cpp',
    'testScene.cpp',
    'testShape.cpp',
    'testSwCanvas.cpp',
    'testSwEngine.cpp',
    'testText.cpp'
]

tests = executable('tvgUnitTests',
    test_file,
    include_directories : headers,
    link_with : thorvg_lib,
    cpp_args : test_compiler_flags,
    dependencies : test_dep)

test('Unit Tests', tests, args : ['--success'])
