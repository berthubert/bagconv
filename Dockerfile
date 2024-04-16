FROM debian:12

ARG DEBIAN_FRONTEND=noninteractive

# install: php / mysql / postgres / sqlite / tools / mssql deps
RUN apt-get update && apt-get -y install \
    build-essential cmake sqlite3 libsqlite3-dev nlohmann-json3-dev zlib1g-dev wget unzip 7zip

# install run script
RUN useradd --create-home appuser
USER appuser
WORKDIR /home/appuser
CMD /home/appuser/run.sh
