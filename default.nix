{ sources ? import ./nix/sources.nix
, nixpkgs ? sources.nixpkgs
, pkgs ? import nixpkgs { }
, buildType ? "Debug" }:

(pkgs.callPackage ./nix/release.nix { inherit buildType; }).nova.gcc9
