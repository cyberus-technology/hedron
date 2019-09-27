let
  rev = "b8f84b24deeccdcd7938d1dca0b347232a4aa022";
  sha256 = "163xban5igwdvay1iqj5kncsym4p099syq2vkpn05a19z6yhp41i";
  url = "https://github.com/NixOS/nixpkgs/archive/${rev}.tar.gz";
in
  builtins.fetchTarball { inherit url sha256; }
