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
- NCM username, cookie file, and `work-dir`

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
- libcurl development headers

On Debian / Ubuntu:

```bash
sudo apt install build-essential make cmake autoconf automake libtool pkg-config libcurl4-openssl-dev
```

On Fedora / RHEL:

```bash
sudo dnf install gcc gcc-c++ make cmake autoconf automake libtool pkg-config libcurl-devel
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

Copy `example.ini` to `config.ini` and edit it:

```bash
cp example.ini config.ini
```

Important NCM settings:

```ini
[ncm]
username='YourNetEaseNickname'
cookie-file=.ncm-cookie
work-dir=lib/netease
```

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

### NCM	

#### `ncm:work-dir`

`work-dir` must point to the directory containing:

```text
lib/netease/server.js
lib/netease/package.json
lib/netease/node_modules
```

For example, when running `mxrec` from the repository root:

```ini
work-dir=lib/netease
```

You can also use an absolute path:

```ini
work-dir=/home/user/mxrec/lib/netease
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

Make sure `config.ini` has the correct NCM working directory:

```ini
[ncm]
work-dir=lib/netease
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
work-dir=lib/netease
```
