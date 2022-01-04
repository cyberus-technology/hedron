{ runCommandNoCC
, clang-tools
, lib
, nix-gitignore
}:

runCommandNoCC "hedron-style-check"
{
  nativeBuildInputs = [ clang-tools ];
  src = lib.cleanSourceWith {
      name = "hedron-source";
      filter = nix-gitignore.gitignoreFilterPure (_: _: true) [ ".git" "nix" ../.gitignore ] ./..;
      src = ./..;
  };
} ''
    cpp_regex='.*\.\(cpp\|hpp\)$'
    files=$(find "$src" -regex "$cpp_regex")
    
    # if clang-format finds style violations, it exits immediately. There is no
    # need to check the exit code of clang-format
    clang-format --dry-run --Werror --ferror-limit 1 $files
    
    echo "Style is good."
    touch $out
''
