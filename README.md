# MXREC

A multi-source music recommendation tool.

## Overview

`mxrec` combines recommendation playlists from multiple music sources into one final playlist.

Currently supported sources:

- Netease Music (`ncm`)
- Last.fm API (`lastfmapi`)
- Last.fm Web (`lastfmweb`)

Core capabilities:

- Fetch recommended tracks from multiple music sources.
- Merge them into a final playlist using configurable algorithms.
- Sample individual sources using `head` or `random`.
- Support weighted, uniform, round-robin, priority, proportional, and reservoir merge strategies.

## Usage

### 1. Prepare configuration

```bash
cp example.ini config.ini
```

Edit `config.ini`. At minimum, configure:

- Last.fm username and API key
- NCM username and cookie file; `work-dir` can usually be left empty

See the NCM cookie section below for authentication.

### 2. Get the NCM cookie

Install Netease Node.js dependencies first:

```bash
cd lib/netease
npm install
cd ../..
```

Then run:

```bash
./mxrec --ncm-auth
```

or:

```bash
./mxrec -A
```

Scan the QR code shown in the terminal and save the printed cookie to the configured `cookie-file`.

### 3. Generate a merged playlist

Use the default configuration:

```bash
./mxrec
```

Select two sources:

```bash
./mxrec -S lastfmapi -S ncm
./mxrec -S lastfmapi -S lastfmweb
```

Set the target number of tracks:

```bash
./mxrec -S lastfmapi -S ncm -n 20
```

Specify a config file, merge algorithm, and sampling method:

```bash
./mxrec --config config.ini --merge=round_robin --sample=head -S lastfmapi -S ncm
```

### 4. Show all options

```bash
./mxrec -h
```

```text
Usage: mxrec [options]
Options:
  -c, --config=<file>          Path to config file
  -S <source>                  Enable source (use multiple times)
                               Sources: lastfmapi lastfmweb ncm
  -t, --export-type=<type>     Export type (json)
  -n, --target=<N>             Number of tracks to collect (default 20)
  -p, --show-progress          Show progress bar (default false)
  -T, --enable-threads         Enable threads (default false)
  -A, --ncm-auth               Obtain NCM login cookie and exit
  -s, --disable-strict         Disable strict mode(default false)
  -h, --help                   Show this help
  -V, --version                Show version (Git commit hash)
```

### 5. Example output

After a successful run, the program prints JSON similar to:

```text
mxrec recommended playlist:
{"playlist":[{"track":{"title":"Song A","album":"Album A","artists":[{"name":"Artist A","alias":[]}],"alias":[]},"urls":[]},{"track":{"title":"Song B","album":"Album B","artists":[{"name":"Artist B","alias":[]}],"alias":[]},"urls":[]}]}
```

Each `track` contains the title, album, artist list, and aliases. `urls` contains playable links and may be an empty array for some sources.

## 1. Build dependencies

Building the main program requires:

- `gcc`, `g++`
- `make`
- `cmake`
- `autoconf`
- `automake`
- `libtool`
- `pkg-config`
- `patchelf`
- libcurl development headers

On Debian / Ubuntu:

```bash
sudo apt install build-essential make cmake autoconf automake libtool pkg-config patchelf libcurl4-openssl-dev
```

On Fedora / RHEL:

```bash
sudo dnf install gcc gcc-c++ make cmake autoconf automake libtool pkg-config patchelf libcurl-devel
```

## 2. Project Build

Clone the repository and initialize submodules:

```bash
git clone <repository-url>
cd mxrec
git submodule update --init --recursive
```

Build the main executable:

```bash
make mxrec
```

Build the executable and tests:

```bash
make
```

## 2.1 Install and uninstall

Install `mxrec`, the NCM Node.js module, the custom libcurl library, and a generated config file:

```bash
make install
```

Optional variables:

```bash
make install PREFIX=/usr/local
make install DESTDIR=/tmp/package-root PREFIX=/usr
```

Installed layout:

```text
/usr/local/bin/mxrec
/usr/local/share/mxrec/config.ini
/usr/local/share/mxrec/netease/
/usr/local/lib/mxrec/curl-impersonate/
```

Uninstall:

```bash
make uninstall
```

`make mxrec` installs the Netease Node.js dependencies into `lib/netease/node_modules`, so `make install` copies the complete module without running `npm install` again.

## 3. Netease module

The NCM source depends on `lib/netease`, a Node.js service.

### 3.1 Node.js version

The project uses Express 5 and other modern npm packages, so Node.js `18` or newer is required.

Check the installed version:

```bash
node --version
```

### 3.2 Install and build

```bash
cd lib/netease
npm install
```

This creates `node_modules` and prepares `server.js`.

To run it manually:

```bash
node server.js --mode http --address 127.0.0.1 --port 9900
```

For Unix socket mode:

```bash
node server.js --mode socket --address /tmp/mxrec.socket
```

Normally `mxrec` starts this service automatically according to the `[ncm]` section in the config file.

## 4. Configuration

When `--config` is not provided, `mxrec` looks for its config file in the following order:

1. `$XDG_CONFIG_HOME/mxrec/config` if `XDG_CONFIG_HOME` is set.
2. `$HOME/.config/mxrec/config` otherwise.

For local development, copy `example.ini` to `config.ini` in the repository root and pass it explicitly:

```bash
cp example.ini config.ini
./mxrec -c config.ini
```

Important NCM settings:

```ini
[ncm]
username='YourNetEaseNickname'
cookie-file=.ncm-cookie
work-dir=
```

If `work-dir` is empty, `mxrec` uses the path baked in at compile time via `NCM_DEFAULT_WORK_DIR`.

### Last.fm history window

`[lastfm.api] period` accepts either a relative lookback such as `1week`, or an
absolute local date/time such as `2021-01-01`, `2021-01-01 12:30:00`, or
`2021-01-01T12:30:00`. Date-times may include fractional seconds, which are
truncated. Bare integers mean seconds ago; units such as `s`, `min`, `h`, `day`,
and `week` are supported. Relative lookbacks extending before the Unix epoch
are clamped to epoch zero.

`string2timestamp()` returns the start timestamp (`from`) for either input form.
The API query uses `from`; time scoring uses the elapsed duration `now - from`,
so `1week` gives a scoring scale of 604800 seconds. Invalid dates, overflowing
lookbacks, pre-epoch absolute starts, and starts at or after now are rejected.

### Merge weights

When `merge=weighted`, each source has a weight:

```ini
[lastfm]
merge_weight=1.0

[lastfm.api]
merge_weight=1.0

[lastfm.web]
merge_weight=1.0

[ncm]
merge_weight=1.0
```

Weight resolution:

1. `lastfmapi` first uses `[lastfm.api] merge_weight`.
2. `lastfmweb` first uses `[lastfm.web] merge_weight`.
3. If a source-specific Last.fm weight is omitted, the global `[lastfm] merge_weight` is used.
4. NCM uses `[ncm] merge_weight`.

Weights must be finite and nonnegative, with at least one positive weight when
sources are configured. A zero weight excludes that source from weighted merging.
Weights determine preferred shares; unused quotas are redistributed to sources
that still have candidates.

### Song deduplication

Each source returns at most its requested number of unique songs. Deduplication
can reduce that number; it does not trigger extra network requests. The final
merge continues past duplicates and exhausted sources to return
`min(requested count, eligible unique songs available)`.
Uniqueness applies within each returned playlist; separate runs can recommend
the same songs again.

Song identity uses the title and the set of artist names. Both UTF-8 strings and
`u8s` values use Unicode NFKC normalization, case folding, trimmed whitespace,
and collapsed internal Unicode whitespace. Artist names are sorted and deduplicated,
so their order does not affect identity. Album names, aliases, and URLs are excluded;
punctuation and version suffixes such as `(Live)` remain significant.

For example, `"  Ｓｏｎｇ\tＡ "` and `"song a"` identify the same title.
Artist lists `["Artist B", " Artist A ", "ARTIST A"]` and
`["artist a", "artist b"]` identify the same artist set. The title must also
match for the songs to be considered duplicates. `"Song"` and `"Song (Live)"`
remain distinct.

An independent `track_key` stores length-prefixed normalized bytes and a hash.
Hash matches are checked against the full key, so hash collisions do not merge
different songs. Original display metadata and JSON serialization are unchanged.
Each source retains its first occurrence of a song in the original order.
During merging, the selected occurrence supplies the song's metadata and URLs;
metadata is not combined across occurrences.

All six merge strategies enforce song uniqueness:

| Strategy | Selection and refill behavior |
| --- | --- |
| `uniform` | Assign equal source quotas and redistribute shortages among sources with remaining candidates. |
| `proportional` | Base quotas on each source's unique candidate count and redistribute shortages. |
| `weighted` | Base quotas on configured positive weights and redistribute shortages; zero-weight sources are excluded. |
| `priority` | Consume unique candidates from each source in configured order before moving to the next source. |
| `round_robin` | Take the next available unique song from each source in turn, skipping duplicates and exhausted sources. |
| `reservoir` | Sample from the global set of distinct songs; repeated occurrences do not add selection weight. |

Quota strategies use largest remainders to allocate integer counts, breaking ties
in source order. Source shares are preferences: filling the requested count takes
priority when duplicates or exhausted sources prevent those shares from being met.

For quota, priority, and round-robin strategies, `head` consumes candidates in
order; `random` shuffles candidate indices once and consumes them without replacement.
Source arrays are not rearranged. Reservoir uses its own random selection rather
than the `head`/`random` candidate cursor, and remembers previously displaced songs
so that later duplicates cannot enter the sample again.

Missing, invalid UTF-8, or normalized-empty titles/artist names are rejected in
strict mode and skipped in permissive mode. Source playlists retain ownership
of tracks and URLs; the merged playlist borrows them. Temporary keys and sampling
state are released after merging.

Run the offline deduplication tests without account configuration or a running
NCM service:

```bash
make check-dedup
# Also account for allocations made by the tested code and static libraries:
make check-dedup-memory
```

These tests cover normalization, hash collisions, stable source deduplication,
all six merge strategies, overlapping and exhausted sources, invalid weights,
random-number failures, and unchanged source objects. Source fixtures also verify
refilling past duplicate prefixes and retaining later NCM response candidates.
Allocation accounting checks that tested code and static libraries leave no
outstanding tracked allocations.

### NCM	

#### `ncm:work-dir`

`work-dir` should point to the directory containing:

```text
server.js
package.json
node_modules
```

By default it is empty:

```ini
work-dir=
```

When empty, `mxrec` uses the compile-time default set by the Makefile macro `NCM_DEFAULT_WORK_DIR`. For `make install`, this default is the installed path, for example:

```text
/usr/local/share/mxrec/netease
```

You can still override it explicitly:

```ini
work-dir=/opt/mxrec/netease
```

#### `ncm:cookie-file`

`cookie-file` is the file that contains the Netease Music login cookie.

The file content is a single cookie string, for example:

```text
MUSIC_U=...; __csrf=...
```

If the path is relative, it is resolved from the current working directory of `mxrec`.

## 5. How to get the NCM cookie

`mxrec` provides a dedicated NCM authentication mode.

### 5.1 Prepare the Netease module

```bash
cd lib/netease
npm install
cd ../..
```

Make sure the installed `mxrec` was built with the expected `PREFIX`, or set `work-dir` explicitly in `config.ini`.

If using the default install layout, leave it empty:

```ini
[ncm]
work-dir=
```

### 5.2 Run NCM authentication mode

```bash
./mxrec --ncm-auth
```

or:

```bash
./mxrec -A
```

This mode will:

1. Start the local NCM Node.js service.
2. Request a Netease Music login QR code.
3. Display the QR code in the terminal.
4. Poll the login status until the QR code is scanned or timeout.
5. Print the resulting cookie to stdout.

### 5.3 Save the cookie

After the QR login succeeds, copy the printed cookie into the configured cookie file.

For the default relative path in the repository root:

```bash
printf '%s' '<cookie string>' > .ncm-cookie
```

Then update `example.ini` or `config.ini`:

```ini
[ncm]
username='YourNetEaseNickname'
cookie-file=.ncm-cookie
work-dir=
```
