# ccze-win

A Windows log colorizer inspired by [ccze](https://github.com/cornet/ccze). Reads log data from stdin or a file, applies regex-based color rules, and outputs colorized text to the terminal or as HTML.

## Features

- **ANSI & Windows Console** color auto-detection (VTP or `SetConsoleTextAttribute` fallback)
- **HTML output** mode with inline CSS or external stylesheet
- **PCRE2 regex** rules loaded from a config file
- **Tool rules** that pipe matched text through external commands (e.g. `jq .`)
- **Word coloring** of common keywords (ERROR, WARN, INFO, etc.) in unmatched text
- **Built-in syslog parser** matching ccze's color scheme
- **Syslog facility stripping** (`-r`)
- **Single static binary** — no DLL dependencies

## Quick Start

```
ccze.exe [OPTIONS] [FILE]
```

Pipe a log file:
```cmd
type myapp.log | ccze
```

Read from a file:
```cmd
ccze myapp.log
```

Output as HTML:
```cmd
ccze -h myapp.log > output.html
```

## Options

| Flag | Description |
|------|-------------|
| `-A`, `--raw-ansi` | Force ANSI escape code output |
| `-h`, `--html` | Output colorized HTML |
| `-m`, `--mode MODE` | Output mode: `ansi`, `html`, `none` (default: auto) |
| `-F`, `--rcfile FILE` | Use FILE as config instead of `ccze.conf` |
| `-c`, `--color KEY=COL` | Override a color from the command line |
| `-r`, `--remove-facility` | Strip syslog facility/level prefix |
| `-l`, `--list-rules` | List loaded rules and exit |
| `-o`, `--options OPT` | Toggle: `wordcolor`/`nowordcolor`, `transparent`/`notransparent`, `cssfile=FILE` |
| `--no-color` | Disable all color output |
| `-V`, `--version` | Print version and exit |
| `--help` | Show help |

## Configuration

Rules are loaded from `ccze.conf` (next to `ccze.exe`, or specified with `-F`).

```
# Lines starting with # are comments

# Color rules: color  COLOR_NAME  PCRE2_REGEX
color  BRIGHT_RED     \bERROR\b
color  GREEN          \bINFO\b
color  YELLOW         \bWARN(ING)?\b

# Tool rules: pipe matched text through a command
tool   jq .    \{[\s\S]*?\}
```

Available colors: `BLACK`, `RED`, `GREEN`, `YELLOW`, `BLUE`, `MAGENTA`, `CYAN`, `WHITE`, and `BRIGHT_` variants of each.

Rules are processed in order. First match wins per character position.

## Building

Requires:
- Visual Studio 2022 (MSVC)
- [vcpkg](https://github.com/microsoft/vcpkg) with `pcre2:x64-windows-static`

```cmd
vcpkg install pcre2:x64-windows-static
build.bat
```

Produces a single `ccze.exe` with PCRE2 statically linked.

## License

[PolyForm Noncommercial 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0) — free for personal/non-commercial use. See [LICENSE.md](LICENSE.md) for details.
