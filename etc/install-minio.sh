#!/bin/bash

download_url="https://dl.minio.io/server/minio/release/linux-amd64/minio"
dest="testbin/minio"
mkdir "testbin" || true
curl -L $download_url > $dest
chmod +x $dest

download_url="https://dl.minio.io/client/mc/release/linux-amd64/mc"
dest="testbin/mc"
curl -L $download_url > $dest
chmod +x $dest
