/**
 * Build & Test Watcher Extension for PhoenixBasic
 *
 * Automatically runs `make test-all` after modifications to source files.
 * Shows a notification with the test results summary.
 * Can be configured to run after every turn or on demand.
 */

import { spawnSync } from "node:child_process";
import type { ExtensionAPI, ExtensionContext } from "@earendil-works/pi-coding-agent";

const SOURCE_EXTENSIONS = new Set([".c", ".h", ".bas", ".md", "Makefile"]);

function isSourceFile(path: string): boolean {
  return SOURCE_EXTENSIONS.has(path.slice(path.lastIndexOf(".")));
}

function runBuild(cwd: string): { success: boolean; output: string } {
  const result = spawnSync("make", ["test-all"], {
    cwd,
    stdio: "pipe",
    encoding: "utf-8",
    timeout: 30_000,
  });

  return {
    success: result.status === 0,
    output: result.stdout + result.stderr,
  };
}

export default function (pi: ExtensionAPI) {
  let sourceModifiedThisTurn = false;

  pi.on("turn_start", async () => {
    sourceModifiedThisTurn = false;
  });

  pi.on("tool_call", async (event, ctx: ExtensionContext) => {
    // Track if any source files are being modified
    if (event.toolName === "write" || event.toolName === "edit") {
      const input = event.input as { path?: string; file_path?: string };
      const filePath = input.path || input.file_path || "";

      if (isSourceFile(filePath)) {
        sourceModifiedThisTurn = true;
      }
    }
  });

  pi.on("agent_end", async (_event, ctx: ExtensionContext) => {
    if (!sourceModifiedThisTurn) return;

    ctx.ui.notify("🔨 Running make test-all...", "info");

    const { success, output } = runBuild(ctx.cwd);

    // Extract the last 3 lines for a summary
    const lines = output.trim().split("\n");
    const summary = lines.slice(-3).join("\n");

    if (success) {
      ctx.ui.notify(`✅ All tests passed!\n${summary}`, "success");
    } else {
      ctx.ui.notify(`❌ Tests FAILED!\n${summary}`, "error");
    }
  });

  // Manual command to run tests
  pi.registerCommand("test", {
    description: "Run make test-all now",
    handler: async (_args, ctx) => {
      ctx.ui.notify("🔨 Running make test-all...", "info");

      const { success, output } = runBuild(ctx.cwd);
      const lines = output.trim().split("\n");
      const summary = lines.slice(-5).join("\n");

      if (success) {
        ctx.ui.notify(`✅ All tests passed!\n${summary}`, "success");
      } else {
        ctx.ui.notify(`❌ Tests FAILED!\n${summary}`, "error");
      }
    },
  });

  // Manual build command (compile only)
  pi.registerCommand("build", {
    description: "Run make (compile only)",
    handler: async (_args, ctx) => {
      ctx.ui.notify("🔨 Running make...", "info");

      const result = spawnSync("make", {
        cwd: ctx.cwd,
        stdio: "pipe",
        encoding: "utf-8",
        timeout: 30_000,
      });

      if (result.status === 0) {
        ctx.ui.notify("✅ Build successful", "success");
      } else {
        const lines = (result.stdout + result.stderr).trim().split("\n");
        const errors = lines.filter((l) => l.includes("error:") || l.includes("warning:"));
        const summary = errors.slice(-5).join("\n");
        ctx.ui.notify(`❌ Build failed!\n${summary}`, "error");
      }
    },
  });
}
