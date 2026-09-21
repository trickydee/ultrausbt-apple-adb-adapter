# Contributing

## Build

- Default release image: `./build-all.sh` → `dist/`
- Quick iterate: `./build.sh`
- If `PICO_SDK_PATH` is unset, scripts may clone under `.pico-sdk/` (gitignored).
- Optional: `git submodule update --init --recursive`

Recommended board for the full feature set: **Pico 2 W** (Bluetooth + ADB Device/Host toggle).

## Please do not commit

- `dist/`, `build*/`, `.pico-sdk/`, `dustbin/`
- Large uncompressed photos; compress under `images/` first
- Local-only notes such as `docs/branches-to-clear.md` or `ORG_TEMPLATE.local.md`

## Docs

- User guides stay in `docs/` top level
- History → `docs/archive/`; captures → `docs/fixtures/`
- Prefer firmware **version** numbers over branch names in user-facing text

## Pull requests

- Target `main`
- Note board(s) tested and **ADB Device** vs **ADB Host** mode
- Update `docs/release-notes.md` when behaviour changes
