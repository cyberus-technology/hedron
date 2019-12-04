{
  nixpkgs ? import ./nix/nixpkgs.nix,
  pkgs ? import nixpkgs {},
  buildType ? "Debug"
}:
(pkgs.callPackage ./nix/release.nix { inherit buildType; }).nova.gcc9
