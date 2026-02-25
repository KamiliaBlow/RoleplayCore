#!/bin/bash
set -e
apt-get update
apt-get install -y --no-install-recommends build-essential cmake ninja-build libboost-all-dev libmysqlclient-dev libssl-dev zlib1g-dev libbz2-dev libreadline-dev libncurses-dev clang
echo "TrinityCore build dependencies installed."
