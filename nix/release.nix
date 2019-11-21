# This file contains Nix expressions for building NOVA with different
# compilers and with different tools.
#
# To build NOVA with all supported compilers, you can do (from the repository root)
#     nix-build nix/release.nix -A nova
#
# To build NOVA only with a specific compiler, you can specify this as
# well:
#     nix-build nix/release.nix -A nova.clang_8
#
# Integration tests work similarly:
#     nix-build nix/release.nix -A integration-test
#     nix-build nix/release.nix -A integration-test.gcc8
#
# To build a unit test coverage report, build the coverage attribute:
#     nix-build nix/release.nix -A coverage
{
  nixpkgs ? import ./nixpkgs.nix,
  nurpkgs ? import ./nur-packages.nix,
  pkgs ? import nixpkgs {},
  nur ? import nurpkgs { inherit pkgs; },
  buildType ? "Debug"
}:

let
  nova = import ./build.nix;
  itest = import ./integration-test.nix;
  cmake-modules = pkgs.callPackage ./cmake-modules.nix {};
  compilers = { inherit (pkgs) clang_8 clang_9 gcc7 gcc8 gcc9; };
  novaBuilds = with pkgs; lib.mapAttrs (_: v: callPackage nova {
    stdenv = overrideCC stdenv v;
    inherit buildType;
  }) compilers;
  testBuilds = with pkgs; lib.mapAttrs (_: v: callPackage itest { nova = v; })
    novaBuilds;
in
  rec {
    nova = novaBuilds;
    integration-test = testBuilds;

    coverage = nova.gcc9.overrideAttrs (old: {
      name = "nova-coverage";
      cmakeBuildType = "Debug";
      cmakeFlags = [
        "-DCOVERAGE=true"
        "-DCMAKE_MODULE_PATH=${cmake-modules}"
        "-DCMAKE_BUILD_TYPE=Debug"
      ];

      checkInputs = old.checkInputs ++ (with pkgs; [ gcovr python3 python3Packages.setuptools ]);

      makeFlags = [ "test_unit_coverage" ];
      installPhase = ''
        mkdir -p $out/nix-support
        cp -r test_unit_coverage/* $out/
        echo "report coverage $out index.html" >> $out/nix-support/hydra-build-products
      '';
    });
  }
