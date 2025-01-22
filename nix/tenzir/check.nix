{
  lib,
  stdenvNoCC,
  src,
  tenzir-integration-test-deps,
  pkgsBuildBuild,
}:
# The untested tenzir edition build.
unchecked:

stdenvNoCC.mkDerivation {
  inherit (unchecked) pname version meta;
  inherit src;

  dontBuild = true;
  strictDeps = true;

  doCheck = true;
  nativeCheckInputs = tenzir-integration-test-deps;
  checkPhase =
    let
      py3 = pkgsBuildBuild.python3.withPackages (ps: [
        ps.datetime
      ]);
      template = path: ''
        if [ -d "${path}/bats/tests" ]; then
          echo "running ${path} BATS tests"
          bats -T -j $(nproc) "${path}/bats/tests"
        fi
        if [ -f "${path}/tests/run.py" ]; then
          echo "running ${path} integration tests"
          ${path}/tests/run.py -j $(nproc)
        fi
      '';
    in
    ''
      patchShebangs tenzir/bats/data/misc/scripts
      export PATH=''${PATH:+$PATH:}${lib.getBin unchecked}/bin:${lib.getBin pkgsBuildBuild.toybox}/bin
      export BATS_LIB_PATH=''${BATS_LIB_PATH:+''${BATS_LIB_PATH}:}$PWD/tenzir/bats
      export PYTHONPATH=''${PYTHONPATH:+''${PYTHONPATH}:}${py3}/${py3.sitePackages}
      mkdir -p cache
      export XDG_CACHE_HOME=$PWD/cache
      ${template "tenzir"}
      ${lib.concatMapStrings template unchecked.plugins}
    '';

  # We just symlink all outputs of the unchecked derivation.
  inherit (unchecked) outputs;
  installPhase = ''
    runHook preInstall;
    ${lib.concatMapStrings (o: "ln -s ${unchecked.${o}} ${"$"}${o}; ") unchecked.outputs}
    runHook postInstall;
  '';
  dontFixup = true;
  passthru = unchecked.passthru // {
    inherit unchecked;
  };
}
