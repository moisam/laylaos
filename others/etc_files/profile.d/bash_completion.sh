# /etc/profile        - systemwide environment variables and startup progs
# /etc/bashrc         - systemwide aliases and functions
# /etc/profile.d/*.sh - individual init scripts
# ~/.bash_profile     - personal environment variables and startup progs
# ~/.bashrc           - personal aliases and functions

# Check for interactive bash and that we haven't already been sourced.
if [ "x${BASH_VERSION-}" != x -a "x${PS1-}" != x -a "x${BASH_COMPLETION_VERSINFO-}" = x ]; then

    # Check for recent enough version of bash.
    if [ "${BASH_VERSINFO[0]}" -gt 4 ] ||
        [ "${BASH_VERSINFO[0]}" -eq 4 -a "${BASH_VERSINFO[1]}" -ge 2 ]; then
        [ -r "$HOME/.config/bash_completion" ] &&
            . "$HOME/.config/bash_completion"
        if shopt -q progcomp && [ -r /usr/share/bash-completion/bash_completion ]; then
            # Source completion code.
            . /usr/share/bash-completion/bash_completion
        fi
    fi

fi

