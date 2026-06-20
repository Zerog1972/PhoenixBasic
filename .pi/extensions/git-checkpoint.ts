/**
 * Git Checkpoint Extension for PhoenixBasic
 *
 * Automatically creates a git stash/checkpoint before each agent turn
 * so that changes can be inspected or reverted if needed.
 *
 * On agent_start: stashes any uncommitted changes with a descriptive message
 * On agent_end: optionally commits or leaves stash for user review
 */

import { execSync, spawnSync } from "node:child_process";
import type { ExtensionAPI, ExtensionContext } from "@earendil-works/pi-coding-agent";

function isGitRepo(cwd: string): boolean {
  try {
    const result = spawnSync("git", ["rev-parse", "--git-dir"], { cwd, stdio: "pipe" });
    return result.status === 0;
  } catch {
    return false;
  }
}

function hasUncommittedChanges(cwd: string): boolean {
  try {
    const status = execSync("git status --porcelain", { cwd, encoding: "utf-8" });
    return status.trim().length > 0;
  } catch {
    return false;
  }
}

function getDiffStat(cwd: string): string {
  try {
    const diff = execSync("git diff --stat", { cwd, encoding: "utf-8" });
    const staged = execSync("git diff --cached --stat", { cwd, encoding: "utf-8" });
    return [diff.trim(), staged.trim()].filter(Boolean).join("\n");
  } catch {
    return "";
  }
}

function createStash(cwd: string, label: string): boolean {
  try {
    execSync(`git stash push -m "pi-checkpoint: ${label}"`, { cwd, stdio: "pipe" });
    return true;
  } catch {
    return false;
  }
}

function popStash(cwd: string): boolean {
  try {
    execSync("git stash pop", { cwd, stdio: "pipe" });
    return true;
  } catch {
    return false;
  }
}

function getLastStashMessage(cwd: string): string {
  try {
    const msg = execSync('git stash list -1 --format="%s"', { cwd, encoding: "utf-8" });
    return msg.trim();
  } catch {
    return "";
  }
}

export default function (pi: ExtensionAPI) {
  let checkpointCreatedThisTurn = false;
  let hasCheckpoint = false;

  pi.on("turn_start", async (_event, ctx: ExtensionContext) => {
    if (!isGitRepo(ctx.cwd)) return;
    checkpointCreatedThisTurn = false;
  });

  pi.on("tool_call", async (event, ctx: ExtensionContext) => {
    if (!isGitRepo(ctx.cwd)) return;
    if (checkpointCreatedThisTurn) return;

    // Only checkpoint before write/edit commands that modify source files
    const writeTools = ["write", "edit", "bash"];
    if (!writeTools.includes(event.toolName)) return;

    // For bash, only checkpoint for dangerous patterns
    if (event.toolName === "bash") {
      const cmd = (event.input as any).command || "";
      const isModifying = /(make|gcc|rm\s+-rf|git\s+commit|git\s+push|install)/.test(cmd);
      if (!isModifying) return;
    }

    if (!hasUncommittedChanges(ctx.cwd)) return;

    // Create a checkpoint
    const diffStat = getDiffStat(ctx.cwd);
    const label = `pre-${event.toolName} at ${new Date().toISOString().slice(0, 19)}`;
    const ok = createStash(ctx.cwd, label);

    if (ok) {
      checkpointCreatedThisTurn = true;
      hasCheckpoint = true;
      ctx.ui.notify(`📦 Git checkpoint created: ${label}`, "info");
    }
  });

  pi.on("agent_end", async (_event, ctx: ExtensionContext) => {
    if (!isGitRepo(ctx.cwd) || !hasCheckpoint) return;

    hasCheckpoint = false;
    const stashMsg = getLastStashMessage(ctx.cwd);
    if (stashMsg.includes("pi-checkpoint:")) {
      ctx.ui.notify(
        `💡 Git checkpoint available: git stash apply (message: "${stashMsg}")`,
        "info",
      );
    }
  });

  // Register a command to restore the checkpoint
  pi.registerCommand("checkpoint-restore", {
    description: "Restore the last git checkpoint created by this extension",
    handler: async (_args, ctx) => {
      if (!isGitRepo(ctx.cwd)) {
        ctx.ui.notify("Not a git repository", "error");
        return;
      }

      const stashList = execSync("git stash list", { cwd: ctx.cwd, encoding: "utf-8" });
      const piStashes = stashList
        .split("\n")
        .filter((l) => l.includes("pi-checkpoint:"));

      if (piStashes.length === 0) {
        ctx.ui.notify("No pi checkpoints found", "info");
        return;
      }

      const confirmed = await ctx.ui.confirm(
        "Restore checkpoint?",
        `This will apply: ${piStashes[0]}\nUncommitted changes will be lost. Continue?`,
      );
      if (!confirmed) return;

      // Drop the stash so it applies cleanly
      spawnSync("git", ["stash", "apply"], { cwd: ctx.cwd, stdio: "pipe" });
      ctx.ui.notify("✅ Checkpoint restored", "success");
    },
  });

  pi.registerCommand("checkpoint-list", {
    description: "List all pi checkpoints",
    handler: async (_args, ctx) => {
      if (!isGitRepo(ctx.cwd)) {
        ctx.ui.notify("Not a git repository", "error");
        return;
      }

      const stashList = execSync("git stash list", { cwd: ctx.cwd, encoding: "utf-8" });
      const piStashes = stashList
        .split("\n")
        .filter((l) => l.includes("pi-checkpoint:"));

      if (piStashes.length === 0) {
        ctx.ui.notify("No pi checkpoints found", "info");
      } else {
        ctx.ui.notify(
          `📦 ${piStashes.length} checkpoint(s):\n${piStashes.join("\n")}`,
          "info",
        );
      }
    },
  });
}
