{ sources ? import ./nix/sources.nix
, nixpkgs ? sources.nixpkgs
, pkgs ? import nixpkgs { }
}:

(pkgs.callPackage ./nix/release.nix { inherit sources nixpkgs pkgs; }).nova.default-debug
