{
  nixpkgs ? import ./nixpkgs.nix,
  nurpkgs ? import ./nur-packages.nix,
  pkgs ? import nixpkgs {},
  nur ? import nurpkgs { inherit pkgs; }
}:

let
  nova = import ./build.nix;
  itest = import ./integration-test.nix;
  cmake-modules = pkgs.callPackage ./cmake-modules.nix {};
  compilers = { inherit (pkgs) clang_8 gcc7 gcc8 gcc9; };
  novaBuilds = with pkgs; lib.mapAttrs (_: v: callPackage nova {
      stdenv = overrideCC stdenv v;
    }) compilers;
  testBuilds = with pkgs; lib.mapAttrs (_: v: callPackage itest { nova = v; })
    novaBuilds;
in
  rec {
    nova = novaBuilds;
    integration-test = testBuilds;

    coverage = nova.gcc9.overrideAttrs (old: {
      name = "nova-coverage";
      cmakeFlags = [
        "-DCOVERAGE=true"
        "-DCMAKE_MODULE_PATH=${cmake-modules}"
      ];
      nativeBuildInputs = old.nativeBuildInputs ++
        (with pkgs; [ gcovr python3 ]);
      makeFlags = [ "test_unit_coverage" ];
      installPhase = ''
        mkdir -p $out/nix-support
        cp -r test_unit_coverage/* $out/
        echo "report coverage $out index.html" >> $out/nix-support/hydra-build-products
      '';
    });
  }
