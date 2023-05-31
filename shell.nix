{ sources ? import ./nix/sources.nix
, nixpkgs ? sources.nixpkgs
, pkgs ? import nixpkgs { }}:

let
  release = import ./nix/release.nix { inherit sources pkgs; };
in
pkgs.mkShell {

  # A compiler invoked from a nix-shell is a wrapper which may add unexpected
  # compile flags to harden the build. One example is `-fPIC` which causes the
  # release build to fail with:
  #
  #   lto1: error: code model kernel does not support PIC mode
  #
  # These can be debugged via `NIX_DEBUG=1 <command>`.
  hardeningDisable = [ "all" ];

  inputsFrom = [
    release.hedron.builds.default-debug
    release.hedron.stylecheck
    release.hedron.docs
  ];

  buildInputs = [
    # To manipulate Nix dependencies.
    pkgs.niv

    # To modify cmake build variables with ccmake.
    pkgs.cmakeCurses

    # Prevent that CLion IDE complains about a missing dependency.
    pkgs.catch2
  ];
}
