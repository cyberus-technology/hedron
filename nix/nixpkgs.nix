let
  rev = "82af060040c4e127cd6819cc1c24afbf33b20c91";
  sha256 = "0w8m70fq0imz07x0g1kjb81i0whga3gqmcnq5fz2119d2f9yllhc";
  url = "https://github.com/NixOS/nixpkgs/archive/${rev}.tar.gz";
in
  builtins.fetchTarball { inherit url sha256; }
