# MXREC

A multi-source music recommendation tool.

## 1. Build dependencies

Building the main program requires:

- `gcc`, `g++`
- `make`
- `cmake`
- `pkg-config`
- libcurl development headers

On Debian / Ubuntu:

```bash
sudo apt install build-essential make cmake pkg-config libcurl4-openssl-dev
```

On Fedora / RHEL:

```bash
sudo dnf install gcc gcc-c++ make cmake pkg-config libcurl-devel
```

## 2. Build

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

### `ncm:work-dir`

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

### `ncm:cookie-file`

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
