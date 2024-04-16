#!/bin/bash
set -e
docker build . -t "bagconv:docker"
docker run -v .:/home/appuser --name "bagconv_docker" "bagconv:docker"
docker rm "bagconv_docker"
docker rmi "bagconv:docker"
