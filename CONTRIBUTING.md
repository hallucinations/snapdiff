# Contributing to snapdiff

## Getting started

```sh
git clone <repo>
cd snapdiff
./configure
make
make test        # all tests must pass before and after your change
```

Dependencies: a C11 compiler, OpenSSL (`libcrypto`), and POSIX pthreads.

## Making changes

- Keep the code C11, no external dependencies beyond OpenSSL and pthreads.
- Match the existing style — no unnecessary comments, no trailing whitespace.
- Every change that affects behaviour needs a test in `tests/`.
- Run `make test` and confirm all suites pass.

## Submitting a pull request

1. Fork the repo and create a branch from `main`.
2. Make your changes with focused commits (one logical change per commit).
3. Open a pull request with a clear description of *why*, not just *what*.

## Reporting a bug

Open an issue with:
- The command you ran and its full output.
- OS, compiler version, and OpenSSL version (`openssl version`).
- A minimal reproducer (a small directory structure if relevant).

## Security issues

Please do **not** open a public issue for security vulnerabilities. Email the
maintainers directly instead.

## License

By contributing you agree your changes will be released under the
[MIT License](LICENSE).
