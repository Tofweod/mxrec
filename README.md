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

### Last.fm diffusion limits and ranking

The final `-n`/`target` controls the merged output size. It no longer controls
how many history seeds Last.fm API expands. `[lastfm.api]` has separate limits:

| Option | Default | Meaning |
| --- | ---: | --- |
| `seed-count` | 20 | Maximum distinct history seeds per attempt, capped by available history |
| `request-budget` | 200 | Actual diffusion HTTP requests per attempt, excluding the initial history fetch |
| `node-budget` | 5000 | Retained path nodes per attempt, including duplicates and history intermediates |
| `diffusion` | 0 | Maximum diffusion depth; artist-to-top-track traversal costs two levels |
| `diff-size` | 5 | Requested track branching size; artist branching uses its square root |

Budgets are shared across all seeds within one attempt. Reaching either budget
ends diffusion successfully and retains collected candidates, with a diagnostic
on stderr. Budget limits can produce fewer candidates than the final target.
Actual request failures still retain partial results and trigger the existing
source failure retry rule. Every retry starts with fresh budgets and cache.
Each seed retains its own DFS stack or BFS queue. Seeds take turns in history
order, processing at most one path node per turn; exhausted frontiers are skipped.
This spreads exploration across seeds before revisiting an earlier seed, while
preserving the local DFS/BFS pop order. One expansion can issue multiple HTTP
requests, so turns balance node visits rather than exact request counts. All
seeds still share the same budgets, identity pool and query cache; a very small
budget can stop before every seed gets a turn.

Each expansion collects available track and artist children before admitting
nodes to the shared node budget. Children are ordered by their full path score;
equal scores retain response order, with track children first on a branch tie.
DFS pushes this order in reverse so its next visit follows the highest-scoring
child. BFS appends children in score order while retaining its FIFO semantics;
the artist branch's extra diffusion level does not turn this into a strict
numeric-depth traversal. When node space runs out, higher-scoring children from
that expansion are retained first. Comparing both branches can spend additional
requests before reaching the node limit; the request budget remains a hard cap.

Diffusion progress consistently reports processed path nodes over processed plus
pending nodes. The label shows started/selected seeds and fully drained seeds
(`done`); budget stops are marked explicitly and retain unfinished frontiers in
the pending count until cleanup.

Each Last.fm API fetch attempt owns one identity pool, shared by its history
and all seed/diffusion path nodes. Nodes borrow an immutable canonical
title/artist key and cached hash from that pool; equivalent songs share the same
key object even when MBIDs are missing or different. Interning normalizes each
incoming title/artist pair before looking up the canonical key. The pool is a
local fetch variable passed explicitly through parsing and diffusion, with no
process-global identity state. Path nodes and history are freed before the pool;
retries start with a fresh pool. A per-attempt query cache reuses successful, valid-JSON
HTTP responses for identical request URLs. Cache hits do not consume request
budget. Failed requests are not cached. Cached responses are parsed into separate
nodes for each path, preserving independent parents, depth and scores. A new
node is discarded before budget admission if its parent chain already contains the
same canonical song and identical raw title, artist and both MBID fields. This
prunes cycles without a visited table; with similarities in `[0,1]` and
nonnegative depth penalties, such a return cannot improve the path score or
remaining depth. Different query variants, independent seeds and paths that
converge without a cycle remain eligible. Retained paths still share the node
budget and remain alive until final scoring and output construction.

`sample=topn` selects the most recently heard distinct songs. `sample=random`
selects weighted seeds without replacement. `random-sample-lambda` is the ratio
of the newest seed's weight to the oldest seed's weight: `1` is uniform, values
above `1` favor recent songs, and values between `0` and `1` favor older songs.
Selected seeds take their turns in history order. This sampling is separate from
`[general] sample`, which samples source candidates during the final merge.

The ranking formula has changed from a weighted mean including a zero-valued
root to:

```text
score = 100 * product(path similarities)
            * exp(-diffusion-lambda * (depth - 1))
            / (1 + score-beta * clamp(seed_age / history_window, 0, 1))
```

The root contributes recency, with no similarity edge. Similarity accepts either
a JSON number or decimal string in `[0,1]`. Artist-to-top-track traversal
contributes the artist similarity once and two diffusion levels. At equal edge
similarity, longer paths cannot gain score merely by adding hops. The highest
scoring path wins when multiple paths reach the same song. `diffusion-lambda`
now controls depth decay and `score-beta` controls root recency decay; both are
finite, nonnegative values, with `0` disabling that factor. Existing tuned values
may need adjustment because the ranking semantics have changed.

After Last.fm API finishes its attempts, all retained batches are ranked together
in descending score order. Duplicate songs keep their highest-scoring occurrence;
equal scores retain batch/item order. This also applies when every attempt fails
but partial candidates remain. Compact ranking records are private to Last.fm
API and are discarded after final ordering. Each attempt still releases its
diffusion graph and identity pool; `playitem` and `playlist_viewer` carry no
recommendation scores. Scores are local to this source and are not compared
against NCM or Last.fm Web. `[general] sample=head` consumes this ranking;
`sample=random` still samples candidates uniformly, and `merge=reservoir` performs
its own random selection regardless of the sampling setting.

`strategy` accepts only `dfs` or `bfs`, and `sample` accepts only `topn` or `random`
in `[lastfm.api]`. Missing, invalid or swapped values fail source initialization.

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

Sources store their effective `merge_weight`; weighted merging reads it directly
without matching source names. Source-specific Last.fm overrides remain explicit
after CLI processing. For omitted overrides, inheritance is recomputed after CLI
processing, so `--lastfm-merge-weight` also updates those inherited values.
`--lastfmapi-merge-weight` and `--lastfmweb-merge-weight` explicitly override their
respective values, including zero. The legacy `merge-weight` spelling is also
accepted in `[lastfm.api]` and `[lastfm.web]`; `merge_weight` takes precedence if
both spellings are present.

Weights must be finite and nonnegative, with at least one positive weight when
sources are configured. A zero weight excludes that source from weighted merging.
Weights determine preferred shares; unused quotas are redistributed to sources
that still have candidates. Invalid weighted-merge settings fail before fetching.

### Song deduplication

Each source independently retries failed fetches until one succeeds or its
`[general] max-try` total attempts are exhausted, including the first attempt.
NCM and Last.fm Web also append successful batches until their own deduplicated
candidate count reaches the target or the same budget runs out. Last.fm API
stops after its first successful diffusion, even if it produced fewer candidates.
Another source filling the final playlist never cancels these failure retries.

`[general] timeout` is the per-request curl timeout in milliseconds for Last.fm
Web and NCM; zero disables it. A request timeout is a failed attempt and uses the
same retry budget. Last.fm API ignores this setting and applies no overall curl
request timeout, so diffusion is not bounded by this configuration value.
Each source stores the effective timeout: Web/NCM inherit the general value,
and Last.fm API sets it to zero. The effective value must fit in curl's `long`
timeout argument.

Recommendation options are validated once per source session, before any source
workers start. Invalid Last.fm API options fail without consuming `max-try` or
issuing requests; configuration diagnostics carry no attempt number. Standalone
`recomm_multi`/`recomm_single` calls validate once per call. Failure retries and
shortage refills reuse the session's already validated, fixed options.

Batches are deduplicated within each source and the merge continues past
duplicates. If the merged result is short, refill-capable sources can contribute
more batches using only their remaining attempts. Failed requests, source-local
shortage refill and merge refill all share one counter per source; it is never
reset between phases. Partial results are retained, including from failed
attempts. A zero max-try is invalid. If sources cannot fill the target, the
available result is printed and the shortfall is reported on stderr.

Each failed or partially failed source attempt reports a bounded, source-specific
chain of errors on stderr. Workers retain each attempt's diagnostics until the
main thread prints them as separate blocks after collection. Attempts with no
diagnostic frames produce no error block. Error output excludes request URLs,
credentials and raw server responses. JSON playlist output remains on stdout.
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
order; `random` selects from each source's unvisited candidates without replacement.
New batches are appended to the unvisited pool; already selected songs remain in
the result. Consequently, early and late batches are not globally uniform. Source
arrays are not rearranged. Reservoir uses its own random selection rather than
the `head`/`random` candidate cursor, and remembers previously displaced songs
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
