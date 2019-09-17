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

set -e

if which apt-get 2>/dev/null; then
    # This is currently necessary, at least on Debian 10, to avoid getting
    # complaints about the 'messagebus' group not existing. Gross, I know
    dpkg-statoverride --remove /usr/lib/dbus-1.0/dbus-daemon-launch-helper

    export DEBIAN_FRONTEND=noninteractive
    apt-get update
    apt-get install -y wget
elif which yum 2>/dev/null; then
    yum install -y wget
else
    echo "Can't install requirements" >&2
    exit 1
fi

export PATH=/usr/local/go/bin:$PATH

GO_ARCH=linux-amd64
GO_VERSION=1.12.9
GO_SHA256=ac2a6efcc1f5ec8bdc0db0a988bb1d301d64b6d61b7e8d9e42f662fbb75a2b9b

wget -O go.tar.gz https://dl.google.com/go/go$GO_VERSION.$GO_ARCH.tar.gz
echo "$GO_SHA256 go.tar.gz" | sha256sum -c -
tar -C /usr/local -xzf go.tar.gz
rm go.tar.gz
chmod -R a+w /usr/local/go
