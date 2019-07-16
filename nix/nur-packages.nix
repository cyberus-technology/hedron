let
  rev = "72f14dae9d949592836cd4d69c9bacb7fa546d5d";
  sha256 = "0r6qp1x0ml836n987b530554ramzqxpy09cw3l3f6cmmvfcgmyk9";
  url = "https://github.com/tfc/nur-packages/archive/${rev}.tar.gz";
in
  builtins.fetchTarball { inherit url sha256; }
