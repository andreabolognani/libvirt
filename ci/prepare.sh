# This script is used to prepare the environment that will be used
# to build libvirt inside the container.
#
# You can customize it to your liking, or alternatively use a
# completely different script by passing
#
#  CI_PREPARE_SCRIPT=/path/to/your/prepare/script
#
# to make.
#
# Note that this script will have root privileges inside the
# container, so it can be used for things like installing additional
# packages.

if which apt-get 2>/dev/null; then
    # This is currently necessary, at least on Debian 10, to avoid getting
    # complaints about the 'messagebus' group not existing. Gross, I know
    dpkg-statoverride --remove /usr/lib/dbus-1.0/dbus-daemon-launch-helper

    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y golang cargo rustc
elif which dnf 2>/dev/null; then
    dnf update -y --refresh
    dnf install -y golang cargo rustc
else
    echo "Can't install requirements" >&2
    exit 1
fi
