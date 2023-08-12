FROM debian:stable-slim
WORKDIR /
RUN sed -i 's/^path-exclude/#path-exclude/g' /etc/dpkg/dpkg.cfg.d/docker
RUN apt-get update
RUN apt-get install -y lighttpd nginx
RUN mkdir -p /run/websockets/
RUN mkdir -p /run/controller/
RUN mkdir -p /run/fastcgi/
RUN chown www-data:www-data /run/fastcgi
RUN chown www-data:www-data /run/websockets
RUN chown www-data:www-data /run/controller
#Development Only
RUN apt-get -y  install nginx ncat vim less procps sudo psmisc curl build-essential g++ man git
RUN curl -o- https://raw.githubusercontent.com/nvm-sh/nvm/v0.39.4/install.sh | bash
RUN mkdir -p /root/boost
RUN curl -L "https://boostorg.jfrog.io/artifactory/main/release/1.82.0/source/boost_1_82_0.tar.gz" -o - | tar -C /root/boost -zxf -
CMD ["/usr/bin/bash"]