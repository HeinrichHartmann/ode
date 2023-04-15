{
  description = "A very basic flake";

  inputs = { gsl.url = "https://mirror.ibcp.fr/pub/gnu/gsl/gsl-2.7.tar.gz"; };

  outputs = { self, nixpkgs }: {

    packages.x86_64-linux.hello = nixpkgs.legacyPackages.x86_64-linux.hello;

    defaultPackage.x86_64-linux = self.packages.x86_64-linux.hello;

  };
}
