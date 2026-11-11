# webtor (vendored)

A browser Tor client compiled to WebAssembly, used by `../../tor.js` so the dashboard can reach
an onion address without Tor Browser. It reaches Tor through the Tor Project's Snowflake bridge.

- Source: [andrewtheguy/webtor-rs](https://github.com/andrewtheguy/webtor-rs), release
  [v0.0.1-alpha.25](https://github.com/andrewtheguy/webtor-rs/releases/tag/v0.0.1-alpha.25)
  (`andrewtheguy-webtor-wasm-0.0.1-alpha.25.tgz`, SHA-256 `eeb1dc4034a84fed09a9ae9942a6b9b7357e368d41d21c0c488f67fbd93fa64d`)
- Licence: MIT, see `LICENSE`
- Files, unchanged from the release:
  - `webtor_wasm.js` SHA-256 `f617a0c657ea62c21bbb00784f24c99656b03c5f730cdc1d28d3498696709c2f`
  - `webtor_wasm_bg.wasm` SHA-256 `ea882e41b6d3630ad17df52203d082f291457eb2569e134a4d84cba8e139275f`

It's experimental alpha software that hasn't had a security audit. To update, replace both
files from a newer release and update the hashes here.
