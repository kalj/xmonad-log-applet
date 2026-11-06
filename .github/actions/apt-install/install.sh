#!/bin/bash
# Install $PACKAGES (and the Build-Depends of $BUILD_DEP) with apt, using
# the packages in $ARCHIVES as a download cache. Afterwards $ARCHIVES holds
# the packages to cache for the next run, and the key to cache them under
# ($CACHE_NAME plus a hash of their file names) is written to $GITHUB_OUTPUT.
set -euo pipefail

# Give up on a stalled download after 20 seconds without data and retry it
sudo tee /etc/apt/apt.conf.d/99ci-network >/dev/null <<'CONF'
Acquire::http::Timeout "20";
Acquire::https::Timeout "20";
Acquire::Retries "3";
CONF

# apt can't give up on a download that is just very slow, which happens with
# some mirror servers. Give each attempt a time limit instead, and try again:
# a new attempt usually gets a fast connection, and keeps the packages the
# earlier ones finished downloading.
retry() {
  local limit=$1; shift
  local attempt
  for attempt in 1 2 3; do
    if sudo timeout "$limit" "$@"; then
      return 0
    fi
    echo "::warning::'$*' failed or took more than $limit seconds (attempt $attempt of 3)"
    sleep 5
  done
  echo "::error::'$*' failed 3 times"
  return 1
}

debs() {
  find "$1" -maxdepth 1 -name '*.deb' -printf '%f\n' | sort
}

retry 120 apt-get update

# Packages from earlier runs (after the update, as some apt configurations
# empty the package cache when updating)
mkdir -p "$ARCHIVES"
echo "Restored $(debs "$ARCHIVES" | wc -l) cached packages"
find "$ARCHIVES" -maxdepth 1 -name '*.deb' -exec sudo cp -t /var/cache/apt/archives/ {} +

# Download what the cache doesn't have, with the time limit, then install
# without one: unpacking large packages can take a while.
# shellcheck disable=SC2086  # $PACKAGES is a list of package names
if [ -n "$PACKAGES" ]; then
  retry 180 apt-get install -y --no-install-recommends --download-only $PACKAGES
fi
if [ -n "$BUILD_DEP" ]; then
  retry 180 apt-get build-dep -y --download-only "$BUILD_DEP"
fi

# Keep the downloaded packages for the next run, minus old versions that the
# archive no longer has
sudo apt-get autoclean -y >/dev/null
find "$ARCHIVES" -maxdepth 1 -name '*.deb' -delete
find /var/cache/apt/archives -maxdepth 1 -name '*.deb' -exec cp -t "$ARCHIVES" {} +
count=$(debs "$ARCHIVES" | wc -l)
echo "Caching $count packages"
{
  echo "count=$count"
  echo "key=apt-$RUNNER_OS-$CACHE_NAME-$(debs "$ARCHIVES" | sha256sum | cut -c1-16)"
} >> "$GITHUB_OUTPUT"

# shellcheck disable=SC2086
if [ -n "$PACKAGES" ]; then
  sudo apt-get install -y --no-install-recommends $PACKAGES
fi
if [ -n "$BUILD_DEP" ]; then
  sudo apt-get build-dep -y "$BUILD_DEP"
fi
