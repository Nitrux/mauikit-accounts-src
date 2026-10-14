project('mauikit-accounts', 'cpp', version: '4.0.5', meson_version: '>=1.0.0', default_options: ['cpp_std=c++17', 'buildtype=release'])

cpp_compiler = meson.get_compiler('cpp')
if cpp_compiler.get_id() == 'gcc' and cpp_compiler.has_argument('-Wno-sfinae-incomplete')
    add_project_arguments('-Wno-sfinae-incomplete', language: 'cpp')
endif
qt6 = import('qt6')
qt = dependency('qt6', version: '>=6.4', modules: ['Core', 'Quick', 'Qml', 'Sql', 'Network', 'DBus'])
kf6_include_prefix = get_option('prefix') / 'include' / 'KF6'
kf6_core = declare_dependency(
    include_directories: include_directories(kf6_include_prefix / 'KCoreAddons'),
    dependencies: [cpp_compiler.find_library('KF6CoreAddons')],
)
kf6_i18n = declare_dependency(
    include_directories: include_directories(kf6_include_prefix / 'KI18n'),
    dependencies: [cpp_compiler.find_library('KF6I18n')],
)
mauikit = dependency('MauiKit4', version: '>=4.0.5')
cpp_args = host_machine.cpu_family() == 'x86_64' ? ['-march=x86-64-v3'] : []
accounts_version_header = configure_file(
    input: 'meson-content.in',
    output: 'accounts_version.h',
    configuration: {'CONTENT': ('''#pragma once
#define Accounts_VERSION_STRING "''' + meson.project_version() + '''"
#define GIT_BRANCH "Stable"
#define GIT_COMMIT_HASH "''' + meson.project_version() + '''"
''')}
)
export_header = configure_file(
    input: 'meson-content.in',
    output: 'accounts_export.h',
    configuration: {'CONTENT': ('''#pragma once
#define ACCOUNTS_EXPORT __attribute__((visibility("default")))
''')}
)
accounts_moc = qt6.preprocess(moc_headers: ['src/code/accountsdb.h', 'src/code/mauiaccounts.h'])
lib = shared_library('MauiKitAccounts4', files('src/code/mauiaccounts.cpp', 'src/code/accountsdb.cpp', 'src/code/moduleinfo.cpp') + accounts_moc + [export_header, accounts_version_header], dependencies: [qt, kf6_core, mauikit], cpp_args: cpp_args, version: '4.0.5', soversion: '4', install: true)
plugin_sources = qt6.preprocess(qresources: 'src/code/accounts.qrc', moc_headers: ['src/code/accounts_plugin.h'])
plugin = shared_module('MauiKitAccounts4plugin', ['src/code/accounts_plugin.cpp'] + plugin_sources, link_with: lib, dependencies: [qt, kf6_i18n, mauikit], install: true, install_dir: get_option('libdir') / 'qt6/qml/org/mauikit/accounts')
qml_dir = get_option('libdir') / 'qt6/qml/org/mauikit/accounts'
install_data(configure_file(
    input: 'meson-content.in',
    output: 'qmldir',
    configuration: {'CONTENT': ('module org.mauikit.accounts\nplugin MauiKitAccounts4plugin\n')}
), install_dir: qml_dir)
install_subdir('src/controls.6', install_dir: qml_dir, strip_directory: true)
install_headers(files('src/code/mauiaccounts.h', 'src/code/moduleinfo.h') + [export_header], subdir: 'MauiKit4/Accounts')
pkg = import('pkgconfig')
pkg.generate(lib, name: 'MauiKit Accounts 4', filebase: 'MauiKitAccounts4', version: meson.project_version(), description: 'MauiKit accounts library', requires: 'MauiKit4')

cmake_targets = configure_file(
    input: 'meson-content.in',
    output: 'MauiKitAccounts4Targets.cmake',
    configuration: {'CONTENT': (('''get_filename_component(_MAUIKIT_PREFIX "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
if(NOT TARGET MauiKitAccounts4)
  add_library(MauiKitAccounts4 SHARED IMPORTED)
  set_target_properties(MauiKitAccounts4 PROPERTIES
    IMPORTED_LOCATION "${_MAUIKIT_PREFIX}/@LIBDIR@/libMauiKitAccounts4.so.4.0.5"
    INTERFACE_INCLUDE_DIRECTORIES "${_MAUIKIT_PREFIX}/include/MauiKit4/Accounts")
endif()
if(NOT TARGET MauiKit4::Accounts)
  add_library(MauiKit4::Accounts ALIAS MauiKitAccounts4)
endif()
if(NOT TARGET MauiKit::Accounts)
  add_library(MauiKit::Accounts ALIAS MauiKitAccounts4)
endif()
''')).replace('@LIBDIR@', (get_option('libdir')))}
)
cmake_config = configure_file(
    input: 'meson-content.in',
    output: 'MauiKitAccounts4Config.cmake',
    configuration: {'CONTENT': ('''include(CMakeFindDependencyMacro)
find_dependency(Qt6 6.4 COMPONENTS Core)
include("${CMAKE_CURRENT_LIST_DIR}/MauiKitAccounts4Targets.cmake")
set(MauiKitAccounts4_FOUND TRUE)
''')}
)
cmake_version = configure_file(
    input: 'meson-content.in',
    output: 'MauiKitAccounts4ConfigVersion.cmake',
    configuration: {'CONTENT': (('''set(PACKAGE_VERSION "@VERSION@")
if(PACKAGE_FIND_VERSION VERSION_LESS_EQUAL PACKAGE_VERSION)
  set(PACKAGE_VERSION_COMPATIBLE TRUE)
endif()
if(PACKAGE_FIND_VERSION VERSION_EQUAL PACKAGE_VERSION)
  set(PACKAGE_VERSION_EXACT TRUE)
endif()
''')).replace('@VERSION@', (meson.project_version()))}
)
install_data([cmake_targets, cmake_config, cmake_version], install_dir: get_option('libdir') / 'cmake' / 'MauiKitAccounts4')
