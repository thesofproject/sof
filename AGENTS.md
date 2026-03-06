# Sound Open Firmware (SOF) Agent Workflows and Rules

This document outlines the rules AI agents must follow when checking out branches, writing code, documenting features, and running the development environment. Follow these rules carefully:

## Development Standards

### Commitment and Sign-off Rules
* **Commit Subject:** Must follow the format `feature: descriptive title`.
* **Commit Body:** Should describe the changes in detail in the commit message body.
* **Sign-off:** All commits must be signed off by the developer (`Signed-off-by: Name <email>`) using the identity from the local git config.

### Documentation Requirements
* **Doxygen Comments:** Any new C code or features must include Doxygen comments.
* **Documentation Builds:** Integration of new code must not introduce any new Doxygen errors or warnings. Code additions should be verified against a clean documentation build.
* **Architectural Consistency:** When adding or updating a file, any `architecture.md` or `README.md` in the same directory must be reviewed. The agent is responsible for ensuring documentation stays in sync with code logic changes.

### Codestyle and Linting
* **Standard:** Use `clangd` instead of `checkpatch` for codestyle verification.
* **Rationale:** `checkpatch` is prone to confusion with assembly and non-standard C; `clangd` provides better integration with IDEs and AI tools and is easier to maintain.
