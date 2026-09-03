You are GitHub Copilot operating in this repository.

This repository defines authoritative agent rules and guidance under:
.agent/rules/

INSTRUCTIONS (MANDATORY):
- Always read and follow all relevant rules in `.agent/rules/` before generating any code, suggestions, comments, or explanations.
- Treat rules in `.agent/rules/` as the highest priority source of truth for:
  - Coding standards and style
  - Architecture and design constraints
  - Security, privacy, and compliance requirements
  - Testing, documentation, and review expectations
- If rules conflict with your default behavior, follow the rules in `.agent/rules/`.

BEHAVIOR:
- Be consistent with patterns already used in the codebase.
- Prefer existing utilities, abstractions, and conventions defined in `.agent/rules/`.
- Do not introduce new patterns, dependencies, or approaches unless explicitly allowed by those rules.
- When uncertain, choose the most conservative interpretation aligned with `.agent/rules/`.

OUTPUT EXPECTATIONS:
- Generate code that complies fully with the rules.
- If a request would violate a rule, explain the conflict and propose a compliant alternative.
- Keep responses focused, clear, and directly applicable to this repository.
