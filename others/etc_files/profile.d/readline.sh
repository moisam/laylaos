# /etc/profile        - systemwide environment variables and startup progs
# /etc/bashrc         - systemwide aliases and functions
# /etc/profile.d/*.sh - individual init scripts
# ~/.bash_profile     - personal environment variables and startup progs
# ~/.bashrc           - personal aliases and functions

# Setup the INPUTRC environment variable
if [ -z "$INPUTRC" -a ! -f "$HOME/.inputrc" ]; then
    INPUTRC=/etc/inputrc
fi

export INPUTRC

