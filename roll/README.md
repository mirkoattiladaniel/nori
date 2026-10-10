# roll

The Nori project and toolchain tool: scaffold, build, run and test projects, and manage their
dependencies and compiler versions. `roll` is itself written in Nori.

```sh
roll init hello     # scaffold a project
roll build          # compile (dev profile; --release for optimized)
roll run            # build, then run
roll check          # type-check only (also reports a TODO/FIXME count)
roll check std/json # type-check one file, module folder, or other project
roll xray           # show the memory plan
roll todo           # list TODO markers in the source
roll fixme          # list FIXME markers in the source
roll list           # list cached dependencies (--sort used|downloaded|name)
roll prune          # remove dependencies unused for 90+ days
```

What it does:

- **Projects**: a `nori.manifest` with build profiles; multi-file projects auto-include (no manual
  imports of your own modules).
- **Fast dev builds**: the `dev` profile compiles each module to a cached object in parallel
  (`incremental = true`), so edits rebuild only what changed; `release` stays whole-program + LTO.
- **Dependencies**: decentralized, git-URL based, with caret semver and a `nori.lock`. No registry.
- **Toolchain management**: multiple compiler versions side by side; a project's `nori = X` selects
  one. `roll install <ver>` downloads and verifies a release, `roll use <ver>` switches, and
  `roll update nori` moves to the newest (see `roll toolchain …`).
- **Extensions**: `roll <x>` runs a `roll-<x>` tool, so the toolchain can grow without touching `roll`
  itself.

## License

Licensed under either of

- Apache License, Version 2.0 ([LICENSE-APACHE](../LICENSE-APACHE))
- MIT license ([LICENSE-MIT](../LICENSE-MIT))

at your option.

## Contributing

Contributions are welcome, but focused on hardening rather than features: hunt for bugs, test and fuzz,
and send small fixes for real defects (crashes, miscompiles, safety or correctness problems). New
features and redesigns are handled by the maintainer. Open an issue to discuss an idea rather than
sending an implementation. See [CONTRIBUTING.md](../CONTRIBUTING.md).
