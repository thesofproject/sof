# Links git hooks, if there are no other hooks already

if(NOT EXISTS "${SOF_ROOT_SOURCE_DIRECTORY}/.git")
	return()
endif()

if(NOT CMAKE_HOST_UNIX)
	return()
endif()

set(pre_commit_hook "${SOF_ROOT_SOURCE_DIRECTORY}/.git/hooks/pre-commit")
set(post_commit_hook "${SOF_ROOT_SOURCE_DIRECTORY}/.git/hooks/post-commit")

if(NOT EXISTS ${pre_commit_hook})
	message(STATUS "Linking git pre-commit hook")
	execute_process(COMMAND ln -s -f -T ../../scripts/sof-pre-commit-hook.sh ${pre_commit_hook})
endif()

if(NOT EXISTS ${post_commit_hook})
	message(STATUS "Linking git post-commit hook")
	execute_process(COMMAND ln -s -f -T ../../scripts/sof-post-commit-hook.sh ${post_commit_hook})
endif()
