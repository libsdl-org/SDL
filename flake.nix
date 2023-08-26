{
  "description" = "";

  outputs = { nixpkgs, ... }: {
    devShells.x86_64-linux = let
      pkgs = nixpkgs.legacyPackages.x86_64-linux;
    in {
      default = pkgs.SDL2.overrideAttrs(old: {
        hardeningDisable = (old.hardeningDisable or []) ++ [ "all" ];
      });
    };
  };
}
