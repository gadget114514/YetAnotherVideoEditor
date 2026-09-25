# Autonomous Execution Guidelines

## Autonomy and Tool Permissions
- **Do not ask user permission before making changes or executing commands.** Proceed with tasks autonomously and continuously until fully complete.
- **Do not request review or confirmation** for intermediate steps, terminal commands, builds, tests, or file edits.
- Execute builds, tests, compiler checks, and fixes proactively.
- Avoid using `ask_question` for routine decisions; make sound engineering decisions and proceed. Only communicate questions if blocked by missing external secrets or fundamentally ambiguous requirements.
