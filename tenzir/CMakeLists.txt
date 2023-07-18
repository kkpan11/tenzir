add_executable(tenzir tenzir.cpp)
TenzirTargetEnableTooling(tenzir)
target_link_libraries(tenzir PRIVATE tenzir::internal tenzir::libtenzir)
TenzirTargetLinkWholeArchive(tenzir PRIVATE tenzir::libtenzir_builtins)
add_executable(tenzir::tenzir ALIAS tenzir)

# Install tenzir in PREFIX/lib and headers in PREFIX/include/tenzir.
install(
  TARGETS tenzir
  EXPORT TenzirTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Development
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}" COMPONENT Runtime
  RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" COMPONENT Runtime)

macro (create_tenzir_symlink name)
  # Implicitly create tenzir-ctl and tenzir-node symlinks when building tenzir.
  # That means that the links get re-created every time tenzir is built, but we
  # tolerate this overhead to avoid the inconvenience of missing links.
  add_custom_command(
    TARGET tenzir
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E create_symlink "./tenzir"
            "$<TARGET_FILE_DIR:tenzir>/${name}"
    COMMENT "Creating ${name} symlink"
    VERBATIM)
  # We want the clean target to remove the symlinks, but TARGET* generator
  # expressions aren't allowed in BYPRODUCTS. We can work around that with a
  # low-level mechanism.
  set_property(
    TARGET tenzir
    APPEND
    PROPERTY ADDITIONAL_CLEAN_FILES "$<TARGET_FILE_DIR:tenzir>/${name}")
  # The generated code in cmake_install.cmake for executable targets is
  # different depending on whether the CMAKE_INSTALL_BINDIR is set to an
  # absolute path or not. We emulate this behavior for the symlinks here so they
  # will always end up in the same location.
  if (TENZIR_ENABLE_RELOCATABLE_INSTALLATIONS)
    install(
      CODE "
      execute_process(COMMAND
        \"${CMAKE_COMMAND}\" -E create_symlink \"./tenzir\"
        \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/${name}\")"
      COMPONENT Runtime)
  else ()
    install(
      CODE "
      execute_process(COMMAND
        \"${CMAKE_COMMAND}\" -E create_symlink \"./tenzir\"
        \"\$ENV{DESTDIR}${CMAKE_INSTALL_FULL_BINDIR}/${name}\")"
      COMPONENT Runtime)
  endif ()
endmacro ()

create_tenzir_symlink(tenzir-node)
create_tenzir_symlink(tenzir-ctl)
create_tenzir_symlink(vast)

# -- example configuration file -----------------------------------------------

TenzirInstallExampleConfiguration(
  tenzir "${PROJECT_SOURCE_DIR}/tenzir.yaml.example" "" "tenzir.yaml")

# -- init system integration ---------------------------------------------------

# TODO: This should be behind an option, and work for Linux as well.

if (${CMAKE_SYSTEM_NAME} STREQUAL "FreeBSD")
  # Install rc.d script on FreeBSD into PREFIX/etc/rc.d.
  # TODO: Technically this can break for relocatable binaries when the install
  # prefix at build-time is different than the install prefix at install-time.
  # The macOS installation below handles this correctly, and the FreeBSD
  # installation should be adapted to work similarly.
  configure_file("${CMAKE_CURRENT_SOURCE_DIR}/services/rc.d/tenzir.in"
                 "${CMAKE_CURRENT_BINARY_DIR}/services/rc.d/tenzir" @ONLY)
  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/services/rc.d/tenzir"
    DESTINATION "${CMAKE_INSTALL_SYSCONFDIR}/rc.d"
    PERMISSIONS OWNER_READ OWNER_WRITE GROUP_READ WORLD_EXECUTE
    COMPONENT Runtime)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Darwin")
  # Install launchd script on macOS into ~/Library/LaunchAgents
  install(
    CODE "\
    if (NOT IS_ABSOLUTE \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}\")
      string(PREPEND CMAKE_INSTALL_PREFIX \"${CMAKE_SOURCE_DIR}/\")
    endif ()
    set(TENZIR_BINARY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/bin/$<TARGET_FILE_NAME:tenzir>\")
    set(TENZIR_WORKING_DIRECTORY \"\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/share/tenzir\")
    message(STATUS
      \"Installing: \$ENV{HOME}/Library/LaunchAgents/com.tenzir.tenzir.plist\")
    configure_file(
      \"${CMAKE_CURRENT_SOURCE_DIR}/services/launchd/com.tenzir.tenzir.plist.in\"
      \"\$ENV{HOME}/Library/LaunchAgents/com.tenzir.tenzir.plist\" @ONLY)"
    COMPONENT Runtime)
elseif (${CMAKE_SYSTEM_NAME} STREQUAL "Linux")
  # Not all Linux distros use systemd, but those we care about at the moment do.
  # TODO: We don't consider CPACK_PACKAGE_INSTALL_PREFIX here, this could
  # potentially be solved with a more involved CPack setup. See also the
  # comment at the beginning of cmake/Package.cmake.
  configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/services/systemd/tenzir-node.service.in"
    "${CMAKE_CURRENT_BINARY_DIR}/tenzir-node.service" @ONLY)
  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/tenzir-node.service"
    DESTINATION "${CMAKE_INSTALL_LIBDIR}/systemd/system/"
    COMPONENT Runtime)
endif ()

# -- integration tests ---------------------------------------------------------

# TODO: Hide integration test suite behind an option that is enabled by default.

file(
  GENERATE
  OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/integration-$<CONFIG>.sh"
  CONTENT
    "#!/bin/sh
     if ! command -v jq >/dev/null 2>&1; then
       >&2 echo 'failed to find jq in $PATH'
       exit 1
     fi
     base_dir=\"${CMAKE_CURRENT_SOURCE_DIR}/integration\"
     env_dir=\"${CMAKE_CURRENT_BINARY_DIR}/integration_env\"
     export app=\"$<IF:$<BOOL:${TENZIR_ENABLE_RELOCATABLE_INSTALLATIONS}>,$<TARGET_FILE:tenzir::tenzir>,${CMAKE_INSTALL_FULL_BINDIR}/$<TARGET_FILE_NAME:tenzir::tenzir>>-ctl\"
     update=\"$<IF:$<BOOL:${TENZIR_ENABLE_UPDATE_INTEGRATION_REFERENCES}>,-u,>\"
     set -e
     if [ ! -f \"$env_dir/bin/activate\" ]; then
       python3 -m venv \"$env_dir\"
     fi
     . \"$env_dir/bin/activate\"
     python3 -m pip install --upgrade pip
     python3 -m pip install -r \"$base_dir/requirements.txt\"
     python3 \"$base_dir/integration.py\" \
      --app \"$app\" \
      --directory tenzir-integration-test \
      --set \"$base_dir\"/tests.yaml \
      \${update} \
      \"$@\"
    ")

add_custom_target(
  integration
  COMMAND /bin/sh "${CMAKE_CURRENT_BINARY_DIR}/integration-$<CONFIG>.sh" "-v"
          "DEBUG"
  USES_TERMINAL)

add_custom_target(
  link-integration-directory ALL
  COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/share/tenzir"
  COMMAND
    ${CMAKE_COMMAND} -E create_symlink "${CMAKE_CURRENT_SOURCE_DIR}/integration"
    "${CMAKE_BINARY_DIR}/share/tenzir/integration"
  COMMENT "Linking integration test directory")

install(
  DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/integration"
  COMPONENT Development
  DESTINATION "${CMAKE_INSTALL_DATADIR}/tenzir"
  PATTERN "integration/tenzir.yaml.example" EXCLUDE)

install(
  FILES "${PROJECT_SOURCE_DIR}/tenzir.yaml.example"
  COMPONENT Development
  DESTINATION "${CMAKE_INSTALL_DATADIR}/tenzir/integration")

# -- add plugins ---------------------------------------------------------------

set(plugins_default "plugins/*")
if (EXISTS contrib/tenzir-plugins/.git)
  list(APPEND plugins_default "contrib/tenzir-plugins/*")
endif ()

set(TENZIR_PLUGINS
    ${plugins_default}
    CACHE STRING "Specify a list of plugins to build with Tenzir (supports
    globbing)")
cmake_dependent_option(
  TENZIR_ENABLE_STATIC_PLUGINS "Force plugins to be linked statically" OFF
  "NOT TENZIR_ENABLE_STATIC_EXECUTABLE" ON)
add_feature_info("TENZIR_ENABLE_STATIC_PLUGINS" TENZIR_ENABLE_STATIC_PLUGINS
                 "force plugins to be linked statically.")
if (TENZIR_PLUGINS)
  foreach (plugin_source_glob IN LISTS TENZIR_PLUGINS)
    if (IS_ABSOLUTE "${plugin_source_glob}")
      list(APPEND plugin_source_globs "${plugin_source_glob}")
    else ()
      list(APPEND plugin_source_globs
           "${PROJECT_SOURCE_DIR}/${plugin_source_glob}")
    endif ()
  endforeach ()
  file(
    GLOB plugin_source_dirs
    LIST_DIRECTORIES ON
    CONFIGURE_DEPENDS ${plugin_source_globs})
  list(SORT plugin_source_dirs)
  foreach (plugin_source_dir IN LISTS plugin_source_dirs)
    if (NOT IS_DIRECTORY "${plugin_source_dir}")
      continue()
    endif ()
    get_filename_component(plugin_binary_dir "${plugin_source_dir}" NAME)
    string(PREPEND plugin_binary_dir "${PROJECT_BINARY_DIR}/plugins/")
    add_subdirectory("${plugin_source_dir}" "${plugin_binary_dir}")
  endforeach ()
endif ()

get_property(TENZIR_BUNDLED_PLUGINS GLOBAL
             PROPERTY "TENZIR_BUNDLED_PLUGINS_PROPERTY")
list(TRANSFORM TENZIR_BUNDLED_PLUGINS PREPEND "\"")
list(TRANSFORM TENZIR_BUNDLED_PLUGINS APPEND "\"")
list(JOIN TENZIR_BUNDLED_PLUGINS "," joined_bundled_plugins)
target_compile_definitions(
  tenzir PRIVATE "TENZIR_BUNDLED_PLUGINS=${joined_bundled_plugins}")

# -- man page ------------------------------------------------------------------

# TODO: Feed the last commit date in. Using the current date breaks
# deterministic builds.
string(TIMESTAMP TENZIR_LAST_COMMIT_DATE)
if (TENZIR_ENABLE_MANPAGES)
  add_custom_target(
    tenzir-manpage
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tenzir.1.md"
    COMMAND
      "${PANDOC}" -s -f gfm -t man -V date:${TENZIR_LAST_COMMIT_DATE}
      "${CMAKE_CURRENT_SOURCE_DIR}/tenzir.1.md" -o
      "${CMAKE_CURRENT_BINARY_DIR}/tenzir.1"
    BYPRODUCTS "${CMAKE_CURRENT_BINARY_DIR}/tenzir.1"
    COMMENT "Generating tenzir.1"
    VERBATIM)
  add_dependencies(tenzir tenzir-manpage)
  install(
    FILES "${CMAKE_CURRENT_BINARY_DIR}/tenzir.1"
    DESTINATION "${CMAKE_INSTALL_MANDIR}/man1"
    COMPONENT Runtime)
endif ()