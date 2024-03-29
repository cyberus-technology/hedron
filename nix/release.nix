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
, pkgs ? import sources.nixpkgs { system = "x86_64-linux"; }
}:

let
  cmake-modules = pkgs.callPackage ./cmake-modules.nix { src = sources.cmake-modules; };
  qemuBoot = pkgs.callPackage ./qemu-boot.nix { };

  attrsToList = pkgs.lib.mapAttrsToList pkgs.lib.nameValuePair;

  # A list of all build configurations we support.
  #
  # There is some magic here to pass along the compiler names, so we
  # can use them in hedronBuilds to create nice attribute names.
  buildConfs = pkgs.lib.cartesianProductOfSets {
    cc = attrsToList {
      inherit (pkgs)
        clang_15
        # This is the current Debian stable compiler.
        gcc10
        # This is the next Debian stable compiler.
        gcc12
        # This is the most current GCC.
        gcc13;
    };
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
  hedron = {
    builds = {
      inherit default-release default-debug;
    } // hedronBuildSet;

    stylecheck = pkgs.callPackage ./stylecheck.nix { };
    clang-tidy = pkgs.callPackage ./clang-tidy.nix {
      hedron = hedronBuildSet.clang_15-debug;
    };

    integration-test = testBuilds;

    coverage = pkgs.callPackage ./coverage.nix {
      hedron = default-debug;
      inherit cmake-modules;
    };

    docs = pkgs.callPackage ./docs.nix {};
  };
}
