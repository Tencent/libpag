
include(GetGitRevisionDescription)


# Create file "revision" which contains the current git revision ID and
# deploy this file during installation and packaging.
#
# filename: Name of generated file
# dest:     Destination path for installation
# 
macro(create_revision_file filename dest)

    # Add a revision file containing the git-head tag for cpack and install
    get_git_head_revision(GIT_REFSPEC GIT_SHA1)

    # Generate a shorter, googlelike variation for rev
    string(SUBSTRING "${GIT_SHA1}" 0 12 GIT_REV)
    file(WRITE ${filename} ${GIT_REV})

endmacro()
