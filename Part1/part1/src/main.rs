extern crate nix;
use nix::unistd::getuid;

fn main() {
  let mut _i = getuid();
  _i = getuid();
  _i = getuid();
  _i = getuid();
}
