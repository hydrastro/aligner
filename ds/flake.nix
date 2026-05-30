{
  description = "ds";

  inputs = { nixpkgs.url = "github:NixOS/nixpkgs/nixpkgs-unstable"; };

  outputs = { self, nixpkgs }:
    let
      systems = [ "x86_64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs systems;
    in {
      packages = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; };
        in rec {
          ds = pkgs.stdenv.mkDerivation {
            pname = "ds";
            version = "0.0.0";
            src = ./.;

            nativeBuildInputs = [ pkgs.gnumake ];

            buildPhase = ''
              make
            '';

            installPhase = ''
              make install PREFIX=$out
            '';

            meta = with pkgs.lib; {
              description = "Data structure library";
              platforms = platforms.unix;
            };
          };

          default = ds;
        });

      devShells = forAllSystems (system:
        let pkgs = import nixpkgs { inherit system; };
        in {
          default = pkgs.mkShell {
            packages = [ pkgs.gcc pkgs.gnumake pkgs.gdb pkgs.valgrind ];
          };
        });

      defaultPackage = forAllSystems (system: self.packages.${system}.default);
    };
}
