{ stdenvNoCC }:
let
  rev = "5893e3eb3aaec104f86ba81ee90b7e9279b74c3f";
  sha256 = "0lf8gld2n9afhbcrqaphlvb0sgkggwxrr98kymv94dr48pshlf6d";
  url = "https://github.com/bilke/cmake-modules/archive/${rev}.tar.gz";
  src = builtins.fetchTarball { inherit url sha256; };
in stdenvNoCC.mkDerivation {
  name = "cmake-modules";
  inherit src;

  patchPhase = ''
    # gcovr itself is a bash script that wraps the python script already
    sed -i 's/''${Python_EXECUTABLE} ''${GCOVR_PATH}/''${GCOVR_PATH}/' CodeCoverage.cmake
  '';

  installPhase = ''
    mkdir $out
    cp -r * $out/
  '';
}
