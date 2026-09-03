# Documentation Rules

The user expects all new features and in-code documentation to adhere to Doxygen standards.

## Doxygen Requirements

1. **Mandatory Documentation:** All new features, functions, and structures must include Doxygen comments describing their purpose, parameters, and return values.
2. **Clean Build:** Any in-code documentation added or modified must build with Doxygen without producing any new errors or warnings.
3. **Format:** Use standard Doxygen formatting tags (e.g., `@brief`, `@param`, `@return` or `\brief`, `\param`, `\return`). Ensure the styling matches the existing codebase conventions.

## Directory Documentation

When creating a new file or modifying an existing one, check if there is an `architecture.md` or `readme.md` (or `README.md`) file in the same directory. If present, evaluate whether the code changes require an update to these documentation files and make the necessary updates to keep them synchronized with the code.
