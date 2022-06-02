# This file contains Nix expressions for building Hedron with different
# compilers and with different tools.
#
# To build Hedron with all supported compilers in all supported build
# configurations, you can do (from the repository root):
#
#     nix-build nix/release.nix -A hedron.builds
#
# To build Hedron only with a specific compiler, you can specify this as
# well:
#
#     nix-build nix/release.nix -A hedron.builds.clang_13-release
#
# The available compilers should autocomplete, if Nix was set up
# correctly in your environment.
#
# Each build variant from above has a corresponding integration-test
# attribute:
#
#     nix-build nix/release.nix -A hedron.integration-test.clang_13-release
#
# To build a unit test coverage report, build the coverage attribute:
#
#     nix-build nix/release.nix -A hedron.coverage
#
# To check if any files need reformatting, run:
#
#     nix-build nix/release.nix -A hedron.stylecheck
#
# To lint the code-base using clang-tidy, run:
#
#     nix-build nix/release.nix -A hedron.clang-tidy
{ sources ? import ./sources.nix
, pkgs ? import sources.nixpkgs {}
}:

let
  cmake-modules = pkgs.callPackage ./cmake-modules.nix { src = sources.cmake-modules; };
  qemuBoot = pkgs.callPackage ./qemu-boot.nix {};

  attrsToList = pkgs.lib.mapAttrsToList pkgs.lib.nameValuePair;

  # A list of all build configurations we support.
  #
  # There is some magic here to pass along the compiler names, so we
  # can use them in hedronBuilds to create nice attribute names.
  buildConfs = pkgs.lib.cartesianProductOfSets {
    cc = attrsToList { inherit (pkgs) clang_12 clang_13 gcc7 gcc8 gcc9 gcc10 gcc11 gcc12; };
    buildType = [ "Debug" "Release" ];
  };

  # Take a list of build configurations and turn them into a set of derivations looking like:
  # { gcc9-release = ...; gcc9-debug = ...; }
  hedronBuildSet = with pkgs; let
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
      _: hedron: pkgs.callPackage ./integration-test.nix {
        inherit hedron qemuBoot;
        grub2 = combinedGrub;
      }
    )
    hedronBuildSet;

  default-release = hedronBuildSet.gcc10-release;
  default-debug = hedronBuildSet.gcc10-debug;
in
{
  hedron = pkgs.recurseIntoAttrs {
    builds = pkgs.recurseIntoAttrs {
      inherit default-release default-debug;
    } // hedronBuildSet;

    inherit default-release;
    stylecheck = pkgs.callPackage ./stylecheck.nix {};
    clang-tidy = pkgs.callPackage ./clang-tidy.nix {
      hedron = hedronBuildSet.clang_13-debug;
    };

    integration-test = pkgs.recurseIntoAttrs testBuilds;

    coverage = pkgs.callPackage ./coverage.nix {
      hedron = default-debug;
      inherit cmake-modules;
    };
  };
}
