{ sources ? import ./nix/sources.nix
, nixpkgs ? sources.nixpkgs
, pkgs ? import nixpkgs { }}:

pkgs.mkShell {

  # A compiler invoked from a nix-shell is a wrapper which may add unexpected
  # compile flags to harden the build. One example is `-fPIC` which causes the
  # NOVA Release build to fail with:
  #
  #   lto1: error: code model kernel does not support PIC mode
  #
  # These can be debugged via `NIX_DEBUG=1 <command>`.
  hardeningDisable = [ "all" ];

  inputsFrom = [ (import ./. { inherit sources nixpkgs pkgs; }) ];

  buildInputs = [ pkgs.niv ];
}
