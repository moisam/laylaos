# /etc/profile        - systemwide environment variables and startup progs
# /etc/bashrc         - systemwide aliases and functions
# /etc/profile.d/*.sh - individual init scripts
# ~/.bash_profile     - personal environment variables and startup progs
# ~/.bashrc           - personal aliases and functions

# Turn off group write permission if the username != groupname for non-root users
if [ "$(id -gn)" != "$(id -un)" -a $EUID -gt 99 ] ; then
  umask 002
else
  umask 022
fi

