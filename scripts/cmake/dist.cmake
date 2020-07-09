# SPDX-License-Identifier: BSD-3-Clause

# This creates a release tarball from a git checkout.
#
# Warning: a ".tarball-version" at the top of the source directory takes
# precedence; even for git checkouts it does! This can be used to make
# the build "more deterministic". See GIT_TAG in version.cmake.

set(TAR_BASENAME sof-${GIT_TAG})
set(TARBALL_PATH_TMP "${PROJECT_BINARY_DIR}/${TAR_BASENAME}.tar")
set(TARBALL_PATH "${PROJECT_BINARY_DIR}/${TAR_BASENAME}.tgz")
set(TARBALL_VERSION_BINARY_PATH "${PROJECT_BINARY_DIR}/${TARBALL_VERSION_FILE_NAME}")

add_custom_target(dist
	COMMAND git archive --prefix=${TAR_BASENAME}/ -o "${TARBALL_PATH_TMP}" HEAD

	# .tarball-version in the top build directory for git users' convenience
	COMMAND ${CMAKE_COMMAND} -E echo "${GIT_TAG}" > "${TARBALL_VERSION_BINARY_PATH}"
	COMMAND ${CMAKE_COMMAND} -E echo "${GIT_LOG_HASH}" >> "${TARBALL_VERSION_BINARY_PATH}"

	# ${TAR_BASENAME}/.tarball-version for the release tarball
	COMMAND ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/${TAR_BASENAME}"
	COMMAND ${CMAKE_COMMAND} -E copy "${TARBALL_VERSION_BINARY_PATH}" "${PROJECT_BINARY_DIR}/${TAR_BASENAME}/${TARBALL_VERSION_FILE_NAME}"
	COMMAND tar rf "${TARBALL_PATH_TMP}" -C "${PROJECT_BINARY_DIR}" "${TAR_BASENAME}/${TARBALL_VERSION_FILE_NAME}"

	COMMAND gzip -9 < "${TARBALL_PATH_TMP}" > "${TARBALL_PATH}"

	WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
	COMMENT "Creating tarball: ${TARBALL_PATH}
Warning: you must invoke make/ninja 'rebuild_cache' when the version changes, 'clean' is not enough.
"
	BYPRODUCTS "$TARBALL_VERSION_BINARY_PATH" "${TARBALL_PATH_TMP}" "${TARBALL_PATH}"
	VERBATIM
	USES_TERMINAL
)
