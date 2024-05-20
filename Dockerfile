FROM debian:latest as base
RUN useradd --system --create-home --home-dir /home/baguser --user-group --uid 1000 --shell /bin/bash baguser
RUN apt update && apt install -y build-essential cmake sqlite3 libsqlite3-dev zlib1g-dev wget unzip nlohmann-json3-dev

FROM base as builder
WORKDIR /build
COPY . .
RUN --mount=type=cache,target=/build/CMakeFiles cmake . && make

FROM builder as bagger
WORKDIR /bagger
SHELL ["/bin/bash", "-c"]
RUN wget https://service.pdok.nl/lv/bag/atom/downloads/lvbag-extract-nl.zip \
    --no-verbose --show-progress --progress=dot:giga && \
    unzip -o lvbag-extract-nl.zip && rm lvbag-extract-nl.zip && \
    # unzip the zips containing zips
    find . -name "*.zip" | xargs -P8 -I{} bash -c "unzip {} && rm {}" && \
    # unzip the zips containing xmls
    find . -name "*.zip" | xargs -P8 -I{} bash -c "unzip {} && rm {}" && \
    /build/bagconv 9999{WPL,OPR,NUM,VBO,LIG,STA,PND}*.xml && \
    rm *.xml && \
    sqlite3 bag.sqlite < /build/mkindx
    # Uncomment to build additional views.
    # sqlite3 bag.sqlite < /build/geo-queries

FROM base as server
WORKDIR /srv
RUN chown baguser:baguser /srv && apt purge -y wget wget build-essential cmake
COPY --from=builder --chown=baguser:baguser /build/bagconv /build/bagserv ./
COPY --from=bagger --chown=baguser:baguser /bagger/bag.sqlite .
USER baguser
CMD ["/srv/bagserv", "1234"]
