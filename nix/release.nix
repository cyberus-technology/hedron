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
, cbspkgs ? import sources.cbspkgs-public {}
, pkgs ? cbspkgs.pkgs
}:

let
  cmake-modules = pkgs.callPackage ./cmake-modules.nix { src = sources.cmake-modules; };
  qemuBoot = pkgs.callPackage ./qemu-boot.nix {};

  attrsToList = pkgs.lib.mapAttrsToList pkgs.lib.nameValuePair;

  # A list of all build configurations we support.
  #
  # There is some magic here to pass along the compiler names, so we
  # can use them in novaBuilds to create nice attribute names.
  buildConfs = pkgs.cbspkgs.lib.cartesian.cartesianProductFromSet {
    cc = attrsToList { inherit (pkgs) clang_9 clang_10 gcc7 gcc8 gcc9 gcc10; };
    buildType = [ "Debug" "Release" ];
  };

  # Take a list of build configurations and turn them into a set of derivations looking like:
  # { gcc9-release = ...; gcc9-debug = ...; }
  novaBuildSet = with pkgs; let
    buildFunction = { cc, buildType }: lib.nameValuePair
      "${cc.name}-${lib.toLower buildType}"
      (
        callPackage ./build.nix {
          inherit buildType;
          stdenv = overrideCC stdenv cc.value;
        }
      );
  in
    builtins.listToAttrs (builtins.map buildFunction buildConfs);

  combinedGrub = pkgs.grub2_efi.overrideAttrs (
    old: {
      # This puts BIOS and EFI stuff into the lib/grub folder, which is the simplest
      # way to convince grub-mkrescue to generate hybrid images, as this search
      # path is baked into the binaries. Just using the -d parameter forces you
      # to choose either format. Hybrid images only work with the baked-in path.
      postInstall = ''
        cp -r ${pkgs.grub2}/lib/grub/* $out/lib/grub/
      '';
    }
  );
  testBuilds = builtins.mapAttrs
    (
      _: nova: pkgs.callPackage ./integration-test.nix {
        inherit nova qemuBoot;
        grub2 = combinedGrub;
      }
    )
    novaBuildSet;

  default-release = novaBuildSet.gcc9-release;
  default-debug = novaBuildSet.gcc9-debug;
in
{
  nova = pkgs.recurseIntoAttrs {
    builds = pkgs.recurseIntoAttrs {
      inherit default-release default-debug;
    } // novaBuildSet;

    inherit default-release;

    integration-test = pkgs.recurseIntoAttrs testBuilds;

    coverage = pkgs.callPackage ./coverage.nix {
      nova = default-debug;
      inherit cmake-modules;
    };
  };
}
