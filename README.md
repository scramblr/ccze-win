# ccze-win v1.2 by scramblr ©2026

A Windows log colorizer inspired by [ccze](https://github.com/cornet/ccze). Reads log data from stdin or a file, applies regex-based color rules, and outputs colorized text to the terminal or as HTML.

<h6><i>
   
NOTE: The ccze.conf configuration can be used on both linux and windows versions of ccze, so if you want you can type the following on your linux box:
```
sudo curl https://raw.githubusercontent.com/scramblr/ccze-win/refs/heads/master/ccze.conf -o /etc/cczerc.new
sudo cp /etc/cczerc /etc/cczerc.orig
sudo mv /etc/cczerc.new /etc/cczerc
```
Now you should be able to enjoy slightly different/newer coloring on all files win and not on your *nix boxes! u decide if it's better!

```cat /var/log/syslog|ccze```
or
```cat /var/log/syslog|ccze -A```
(sometimes without running ANSI mode the screen tends to clear for some reason)
</h4> </i>



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
type (or cat, w/e) myapp.log | ccze
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

Build.bat will produce a single `ccze.exe` binary with PCRE2 statically linked. You just need the .conf file and the .exe file in your %PATH% to run and that's all! Oh and of course never forget the LICENSE and README!! That would be ILLEGAL! lmao. 

## Windows Supported Files
   1. CBS.log (Servicing)
   2. dism.log (Deployment)
   3. setupapi.dev.log (Driver installation - uses >>> and <<<)
   4. setupapi.setup.log (App/OS setup)
   5. WindowsUpdate.log (Update traces)
   6. pfirewall.log (Windows Firewall traffic)
   7. SrtTrail.txt (Startup Repair results)
   8. PFRO.log (Pending File Rename Operations)
   9. setuperr.log (OS Installation errors)
   10. setupact.log (OS Installation actions)
   11. MSI*.log (Windows Installer logs)
   12. DirectX.log (DirectX setup)
   13. DPX.log (Data Package Expander)
   14. ReportingEvents.log (Update reporting)
   15. WinSetup.log (Modern Setup logs)
   16. MoSetup.log (Media Creation Tool / Upgrade)
   17. Diagerr/Diagwrn (System Diagnostics)
   18. VSS logs (Volume Shadow Copy)
   19. WlanMgr.log (Wireless networking)
   20. Shell.log (Explorer/Shell events)

## Linux Supported Files:
All of the same files it previously supported - like:
rsyslog files, nginx, apache, bind, dmesg, etc.

## Agnostic Supported Files:
PHP, JavaScript, ASP, etc. (mostly the same as before)

## Have a request?
If you want something added, file an issue at https://github.com/scramblr/ccze-win

## License

[PolyForm Noncommercial 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0) — free for personal/non-commercial use. See [LICENSE.md](LICENSE.md) for details.
