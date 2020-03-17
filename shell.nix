{ sources ? import ./nix/sources.nix
, nixpkgs ? sources.nixpkgs
, pkgs ? import nixpkgs { }}:

pkgs.mkShell {
  inputsFrom = [ (import ./. { inherit sources nixpkgs pkgs; }) ];

  buildInputs = [ pkgs.niv ];
}
