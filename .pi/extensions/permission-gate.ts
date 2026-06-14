/**
 * Permission Gate Extension for PhoenixBasic
 *
 * Protects against dangerous operations:
 * 1. Blocks rm -rf, sudo, dangerous git commands
 * 2. Protects critical paths (.git, build/, etc.)
 * 3. Confirms before large-scale modifications
 * 4. Tracks file write patterns per turn
 */

import type { ExtensionAPI, ExtensionContext } from "@earendil-works/pi-coding-agent";

const DANGEROUS_PATTERNS = [
  /rm\s+-rf/,
  /sudo\s+/,
  /chmod\s+777/,
  /chown\s+/,
  /mkfs\./,
  /dd\s+if=/,
  /:(){/:|:&};:/,
  />\s*\/dev\/(sda|sdb|sdc|nvme|mmc)/,
  /git\s+push\s+--force/,
  /git\s+reset\s+--hard/,
  /git\s+clean\s+-f[d]?/,
];

const PROTECTED_PATHS = [
  "/.git/",
  "/.gitignore",
  "/node_modules/",
  "~/.pi/",
];

const MODIFY_TOOLS = new Set(["write", "edit", "bash"]);

interface FileOperation {
  path: string;
  tool: string;
}

export default function (pi: ExtensionAPI) {
  let operationsThisTurn: FileOperation[] = [];

  pi.on("turn_start", async () => {
    operationsThisTurn = [];
  });

  pi.on("tool_call", async (event, ctx: ExtensionContext) => {
    // === 1. Guard dangerous bash commands ===
    if (event.toolName === "bash") {
      const input = event.input as { command?: string; timeout?: number };
      if (!input.command) return;

      const cmd = input.command;

      // Check dangerous patterns
      for (const pattern of DANGEROUS_PATTERNS) {
        if (pattern.test(cmd)) {
          ctx.ui.notify(
            `⛔ Blocked dangerous command: ${cmd.slice(0, 80)}`,
            "warning",
          );
          return { block: true, reason: `Dangerous command pattern detected: ${pattern}` };
        }
      }

      // Check for protected path access (rm, mv, etc. on sensitive files)
      for (const protectedPath of PROTECTED_PATHS) {
        if (cmd.includes(protectedPath)) {
          const allowed = await ctx.ui.confirm(
            "⚠️ Protected path access",
            `Command accesses protected path "${protectedPath}". Allow?`,
          );
          if (!allowed) {
            return { block: true, reason: `Protected path: ${protectedPath}` };
          }
        }
      }
    }

    // === 2. Guard write/edit tools ===
    if (event.toolName === "write" || event.toolName === "edit") {
      const input = event.input as { path?: string; file_path?: string };
      const filePath = input.path || input.file_path || "";

      for (const protectedPath of PROTECTED_PATHS) {
        if (filePath.includes(protectedPath)) {
          ctx.ui.notify(
            `⛔ Blocked write to protected path: ${filePath}`,
            "warning",
          );
          return { block: true, reason: `Write to protected path: ${protectedPath}` };
        }
      }

      operationsThisTurn.push({ path: filePath, tool: event.toolName });
    }

    // === 3. Guard against too many modifications per turn ===
    if (MODIFY_TOOLS.has(event.toolName)) {
      if (operationsThisTurn.length >= 10) {
        ctx.ui.notify(
          `⚠️ 10+ modifications this turn - consider a focused approach`,
          "warning",
        );
      }
    }
  });

  pi.on("tool_result", async (event, ctx: ExtensionContext) => {
    // Flag errors that might indicate problems
    if (event.toolName === "bash" && (event as any).isError) {
      const details = (event as any).details;
      if (details?.exitCode && details.exitCode > 0) {
        ctx.ui.notify(
          `⚠️ Bash command exited with code ${details.exitCode}`,
          "warning",
        );
      }
    }
  });

  // Register a command to show current operations
  pi.registerCommand("ops", {
    description: "Show file operations this turn",
    handler: async (_args, ctx) => {
      if (operationsThisTurn.length === 0) {
        ctx.ui.notify("No file operations this turn", "info");
        return;
      }
      const ops = operationsThisTurn
        .map((op) => `  ${op.tool}: ${op.path}`)
        .join("\n");
      ctx.ui.notify(`📁 Operations this turn (${operationsThisTurn.length}):\n${ops}`, "info");
    },
  });
}
