#!/usr/bin/env node
const { createApp } = require("./ncm-recomm");
const { program } = require("commander");
const fs = require("fs");

// ---------------------------------------------------------------------------
// CLI
// ---------------------------------------------------------------------------
program
      .name("mxrec-ncm-lib")
      .description("ncm module for mxrec")
      .version("1.0.0");
program
      .option("-p, --port <NUMBER>", "port for http mode", "9900")
      .option("-a, --address <STRING>", "bind address (http) or socket file path (socket)")
      .option("-m, --mode <TYPE>", "running type: http or socket", "http")
      .parse();

const opts = program.opts();

// ---------------------------------------------------------------------------
// Start
// ---------------------------------------------------------------------------
const app = createApp();

if (opts.mode === "socket") {
      const socketPath = opts.address || "/tmp/mxrec.socket";

      try { fs.unlinkSync(socketPath); } catch (_) { /* ignore */ }

      const server = app.listen(socketPath, () => {
            process.stdout.write(JSON.stringify({
                  event: "listening",
                  mode: "socket",
                  path: socketPath,
            }) + "\n");
      });

      const cleanup = () => {
            try { fs.unlinkSync(socketPath); } catch (_) { /* ignore */ }
            server.close();
      };
      process.on("SIGINT", () => { cleanup(); process.exit(); });
      process.on("SIGTERM", () => { cleanup(); process.exit(); });
} else {
      const port = parseInt(opts.port, 10);
      const address = opts.address || "127.0.0.1";

      app.listen(port, address, () => {
            process.stdout.write(JSON.stringify({
                  event: "listening",
                  mode: "http",
                  address,
                  port,
            }) + "\n");
      });
}
