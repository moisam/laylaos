# /etc/profile        - systemwide environment variables and startup progs
# /etc/bashrc         - systemwide aliases and functions
# /etc/profile.d/*.sh - individual init scripts
# ~/.bash_profile     - personal environment variables and startup progs
# ~/.bashrc           - personal aliases and functions

# Enable color support of ls and add some useful aliases
test -r ~/.dircolors && eval "$(dircolors -b ~/.dircolors)" || eval "$(dircolors -b)"

alias ls='ls --color=auto'
alias ll='ls -alF'
alias la='ls -A'
alias l='ls -CF'

alias grep='grep --color=auto'
alias fgrep='fgrep --color=auto'
alias egrep='egrep --color=auto'

