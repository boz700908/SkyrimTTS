---
name: check-logs
description: Quick manual peek at the last 100 lines of SkyrimNVDA.log + Papyrus.0.log. Use when the user wants a fast status check after testing in-game. For deep diagnosis of a specific bug, use the log-analyzer AGENT instead (it explores the log methodically and explains what happened).
when_to_use: User-triggered only. Run this when the user types `/check-logs` to get a fast summary after a play session. Do NOT auto-invoke when investigating bugs — delegate to the log-analyzer agent for that.
disable-model-invocation: true
---

# Quick Log Check

Quickly analyze the SkyrimNVDA plugin logs without spawning a full subagent.

## Log Locations

- **Plugin log**: Data\SKSE\SkyrimNVDA.log (in Skyrim install folder)
- **Papyrus log**: OneDrive\Documents\My Games\Skyrim Special Edition\Logs\Script\Papyrus.0.log

## Analysis Steps

1. **Read the plugin log** (last 100 lines):
   - Look for ERROR or WARN level messages
   - Check menu open/close events
   - Note any crashes (log stops abruptly)
   - Check scanner, autowalk, quest tracking, crafting, and map entries

2. **Read the Papyrus log** (last 50 lines):
   - Check AutoWalk script entries (SkyrimTTS:AutoWalk)
   - Look for script errors or warnings
   - Note any missing properties

3. **Report summary**:
   - Plugin Log: timestamp of last entry, errors, warnings
   - Papyrus Log: timestamp, AutoWalk status, script errors
   - Issues Found: none or list issues

## Quick Checks

- If logs don't exist: "No logs found. Has the game been launched with SKSE?"
- If logs are old (>1 hour): Note the timestamp, may be stale
- If plugin log shows menu events but no speech: "Plugin loaded but TTS not working - check NVDA is running"
