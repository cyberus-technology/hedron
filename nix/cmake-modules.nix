{ sources, stdenvNoCC }:
stdenvNoCC.mkDerivation {
  name = "cmake-modules";
  src = sources.cmake-modules;

  patchPhase = ''
    # gcovr itself is a bash script that wraps the python script already
    sed -i 's/''${Python_EXECUTABLE} ''${GCOVR_PATH}/''${GCOVR_PATH}/' CodeCoverage.cmake
  '';

  installPhase = ''
    mkdir $out
    cp -r * $out/
  '';
}
