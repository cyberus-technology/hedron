# This file contains Nix expressions for building NOVA with different
# compilers and with different tools.
#
# To build NOVA with all supported compilers in all supported build
# configations, you can do (from the repository root):
#     nix-build nix/release.nix -A nova
#
# To build NOVA only with a specific compiler, you can specify this as
# well:
#     nix-build nix/release.nix -A nova.clang_8-release
#
# Integration tests work similarly:
#     nix-build nix/release.nix -A integration-test
#     nix-build nix/release.nix -A integration-test.gcc8-debug
#
# To build a unit test coverage report, build the coverage attribute:
#     nix-build nix/release.nix -A coverage
{ sources ? import ./sources.nix
, nixpkgs ? sources.nixpkgs
, cbspkgs ? import sources.cbspkgs-public { }
, pkgs ? import nixpkgs { }
}:

let
  nova = import ./build.nix;
  itest = import ./integration-test.nix;
  cmake-modules = pkgs.callPackage ./cmake-modules.nix { inherit sources; };

  attrsToList = pkgs.lib.mapAttrsToList pkgs.lib.nameValuePair;

  # A list of all build configurations we support.
  #
  # There is some magic here to pass along the compiler names, so we
  # can use them in novaBuilds to create nice attribute names.
  buildConfs = cbspkgs.lib.cartesian.cartesianProductFromSet {
    cc = attrsToList { inherit (pkgs) clang_8 clang_9 gcc7 gcc8 gcc9; };
    buildType = [ "Debug" "Release"];
  };

  # Take a list of build configurations and turn them into a set of derivations looking like:
  # { gcc9-release = ...; gcc9-debug = ...; }
  novaBuilds = with pkgs; builtins.listToAttrs (builtins.map
    ({cc, buildType}:
      lib.nameValuePair "${cc.name}-${lib.toLower buildType}" (callPackage nova {
        inherit buildType;

        stdenv = overrideCC stdenv cc.value;
      })
    )
    buildConfs);

  combinedGrub = pkgs.grub2_efi.overrideAttrs (old: {
    # This puts BIOS and EFI stuff into the lib/grub folder, which is the simplest
    # way to convince grub-mkrescue to generate hybrid images, as this search
    # path is baked into the binaries. Just using the -d parameter forces you
    # to choose either format. Hybrid images only work with the baked-in path.
    postInstall = ''
      cp -r ${pkgs.grub2}/lib/grub/* $out/lib/grub/
    '';
  });
  testBuilds = with pkgs; lib.mapAttrs
    (_: v: callPackage itest { grub2 = combinedGrub; nova = v; })
    novaBuilds;
in rec {
  nova = {
    default-release = novaBuilds.gcc9-release;
    default-debug = novaBuilds.gcc9-debug;
  } // novaBuilds;

  integration-test = testBuilds;

  coverage = nova.gcc9-debug.overrideAttrs (old: {
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
