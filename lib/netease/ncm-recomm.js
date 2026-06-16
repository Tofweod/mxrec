const {
      login_qr_key,
      login_qr_create,
      login_qr_check,
      recommend_resource,
      recommend_songs,
      login_status,
      personalized,
} = require("@neteasecloudmusicapienhanced/api");

const express = require("express");

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
function ok(data) {
      return { success: true, code: 0, data };
}

function fail(code, msg) {
      return { success: false, code, msg };
}

// ---------------------------------------------------------------------------
// Routes
// ---------------------------------------------------------------------------
function createApp() {
      const app = express();
      app.use(express.json());

      // --- Login ---
      app.post("/login/qr/key", async (_, res) => {
            try {
                  const result = await login_qr_key();
                  res.json(ok(result));
            } catch (err) {
                  res.json(fail(-1, err.message));
            }
      });

      app.post("/login/qr/create", async (req, res) => {
            try {
                  const paras  = req.body;
                  const result = await login_qr_create(paras);
                  res.json(ok(result));
            } catch (err) {
                  res.json(fail(-1, err.message));
            }
      });

      app.post("/login/qr/check", async (req, res) => {
            try {
                  const paras = req.body;
                  const result = await login_qr_check(paras);
                  res.json(ok(result));
            } catch (err) {
                  res.json(fail(-1, err.message));
            }
      });

      app.post("/login/status", async (req, res) => {
            try {
                  const paras = req.body;
                  const result = await login_status(paras);
                  res.json(ok(result));
            } catch (err) {
                  res.json(fail(-1, err.message));
            }
      });

      // --- Recommend ---
      // recommend song lists
      app.post("/recommend/resource", async (req, res) => {
            try {
                  const paras = req.body;
                  const result = await recommend_resource(paras);
                  res.json(ok(result));
            } catch (err) {
                  res.json(fail(-1, err.message));
            }
      });

      // daily recommend songs
      app.post("/recommend/daily/songs", async (req, res) => {
            try {
                  const paras = req.body;
                  const result = await recommend_songs(paras);
                  res.json(ok(result));
            } catch (err) {
                  res.json(fail(-1, err.message));
            }
      });

      // --- Health ---
      app.post("/health", (_req, res) => res.json(ok(null)));

      return app;
}

module.exports = { createApp };
