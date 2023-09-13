FROM debian:latest as base
RUN useradd --system --create-home --home-dir /home/baguser --user-group --uid 1000 --shell /bin/bash baguser
RUN apt update && apt install -y build-essential cmake sqlite3 libsqlite3-dev zlib1g-dev wget unzip

FROM base as builder
WORKDIR /build
COPY . .
RUN --mount=type=cache,target=/build/CMakeFiles cmake . && make

FROM builder as bagger
WORKDIR /bagger
RUN --mount=type=cache,target=/bagger wget https://service.pdok.nl/lv/bag/atom/downloads/lvbag-extract-nl.zip \
      --no-verbose --show-progress \
      --progress=dot:giga \
      -O lvbag-extract-nl.zip
RUN unzip -o lvbag-extract-nl.zip
SHELL ["/bin/bash", "-c"]
RUN echo 9999{WPL,OPR,NUM,VBO,LIG,STA,PND}*.zip | xargs -n1 -P8 unzip
RUN /build/bagconv 9999{WPL,OPR,NUM,VBO,LIG,STA,PND}*.xml
RUN sqlite3 bag.sqlite < /build/mkindx
# Uncomment to build additional views.
# RUN sqlite3 bag.sqlite < /build/geo-queries 

FROM base as server
WORKDIR /srv
RUN apt purge -y wget unzip build-essential cmake
COPY --from=builder /build/bagconv /build/bagserv .
COPY --from=bagger /bagger/bag.sqlite .
RUN chown baguser:baguser -R .
USER baguser
CMD ["/srv/bagserv", "1234"]
