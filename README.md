# snapdiff

[![CI](https://github.com/hallucinations/snapdiff/actions/workflows/ci.yml/badge.svg)](https://github.com/hallucinations/snapdiff/actions/workflows/ci.yml)
[![Version](https://img.shields.io/github/v/release/hallucinations/snapdiff?label=version)](https://github.com/hallucinations/snapdiff/releases/latest)
[![License: MIT](https://img.shields.io/badge/license-MIT-green)](LICENSE)

> Take a snapshot of a directory. Come back later. See exactly what changed.

`snapdiff` records the state of every file in a directory tree — paths, sizes,
permissions, modification times, and SHA-256 content hashes — into a compact
`.snap` file. Run it again later and it shows you precisely what was added,
removed, modified, or chmod'd.

Useful for:
- Auditing what a script/installer touched on your filesystem
- Tracking config drift in `/etc` between deployments
- Verifying a build process only touches what it should
- Quick forensics after something breaks

---

## Build & Install

```sh
./configure                        # check deps, generate Makefile
./configure --prefix=/usr/local    # custom install prefix
make                               # compile
sudo make install                  # install to PREFIX/bin
```

To uninstall:
```sh
sudo make uninstall
```

---

## Usage

### Take a snapshot
```sh
snapdiff snap /etc -o before.snap
snapdiff snap /var/www/html        # saved as html.snap by default
```

### Diff against a snapshot
```sh
snapdiff diff before.snap /etc
```

Example output:
```
[-] REMOVED  nginx/nginx.conf.bak
[+] ADDED    nginx/sites-enabled/myapp.conf  (1.2 KB)
[~] MODIFIED nginx/nginx.conf
    size:  4.1 KB → 4.3 KB
    hash:  3f8a1b2c4d5e6f7a… → 9c1e2d3f4a5b6c7d…
[M] CHMOD    ssl/private.key (0644 → 0600)

4 change(s) found.
```

Exit codes mirror `diff(1)`:
- `0` — no differences
- `1` — error
- `2` — differences found (useful in scripts)

### Inspect a snapshot
```sh
snapdiff info before.snap
```

---

## Snapshot file format

Plain text, tab-separated — easy to `grep`, `awk`, or parse:

```
# snapdiff v1
# root: /etc
# created: 1713400000
# format: path\tsize\tmode\tmtime\thash
nginx/nginx.conf	4201	644	1713399000	3f8a1b2c...
```

---

## Options

| Flag | Description |
|------|-------------|
| `--prefix=DIR` | Install prefix (configure) |
| `--cc=CC`      | C compiler to use (configure) |
| `-o FILE`      | Output file for `snap` command |

---

## License

MIT
