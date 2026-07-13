---
description: Scout investigate, debuggeur analyse et corrige un bug
---
Use the subagent tool with the chain parameter and agentScope "both" to execute this workflow:

1. First, use the "scout" agent to find all code relevant to: $@
2. Then, use the "debuggeur" agent to analyze and fix the bug described in "$@" using the context from the previous step (use {previous} placeholder)

Execute this as a chain, passing output between steps via {previous}.
