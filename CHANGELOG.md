# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

## [0.1.0] - 2026-04-18

### Added
- `snap` command — capture a directory snapshot to a `.snap` file
- `diff` command — compare a directory against a previously taken snapshot
- `info` command — print metadata (root, creation time, file count, total size) from a snapshot
- `--version` / `-V` flag
- SHA-256 content hashing via OpenSSL EVP
- Parallel file hashing across all available CPU cores (pthreads)
- mtime + size fast-path: unchanged files are not rehashed during `diff`
- Plain-text tab-separated snapshot format, readable with `grep`/`awk`
- Colour-coded diff output: green for added, red for removed, yellow for modified, cyan for chmod
- Exit codes mirror `diff(1)`: 0 = no changes, 1 = error, 2 = changes found
- `./configure` build system with `--prefix` and `--cc` options
- `make install` / `make uninstall` with binary and man page
- Man page (`snapdiff.1`)
- MIT licence

[Unreleased]: https://github.com/hallucinations/snapdiff/compare/v0.1.0...HEAD
[0.1.0]: https://github.com/hallucinations/snapdiff/releases/tag/v0.1.0
