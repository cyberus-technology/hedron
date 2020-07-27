{ hedron
, cmake-modules
, gcovr
, python3Packages
}:
hedron.overrideAttrs (
  old: {
    name = "hedron-coverage";
    cmakeBuildType = "Debug";
    cmakeFlags = [
      "-DCOVERAGE=true"
      "-DCMAKE_MODULE_PATH=${cmake-modules}"
      "-DCMAKE_BUILD_TYPE=Debug"
    ];

    checkInputs = old.checkInputs ++ [ gcovr python3Packages.python python3Packages.setuptools ];

    makeFlags = [ "test_unit_coverage" ];
    installPhase = ''
      mkdir -p $out/nix-support
      cp -r test_unit_coverage/* $out/
      echo "report coverage $out index.html" >> $out/nix-support/hydra-build-products
    '';
  }
)
