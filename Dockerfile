FROM ubuntu:18.04

# Install allowed packages.
RUN apt-get update -qq \
    && apt-get install -y \
                       curl sudo \
                       autoconf=2.69-11 \
                       automake=1:1.15.1-3ubuntu2 \
                       cmake=3.10.2-1ubuntu2.18.04.1 \
                       golang-1.8-go=1.8.3-2ubuntu1.18.04.1 golang-go=2:1.10~4ubuntu1 \
                       ant=1.10.5-3~18.04 \
                       maven=3.6.0-1~18.04.1 \
                       nodejs=8.10.0~dfsg-2ubuntu0.4 \
                       python=2.7.15~rc1-1 \
                       python3=3.6.7-1~18.04 \
                       gcc=4:7.4.0-1ubuntu2.3 \
                       gccgo=4:8.3.0-1ubuntu2.3 \
                       libjemalloc-dev=3.6.0-11 \
                       libboost-dev=1.65.1.0ubuntu1 \
                       clang-5.0=1:5.0.1-4 \
                       libtbb-dev=2017~U7-8 \
                       python-pip=9.0.1-2.3~ubuntu1.18.04.4 \
                       build-essential=12.4ubuntu1 \
                       ruby-full=1:2.5.1 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Install Oracle Java 16.
RUN \
  apt-get update && \
  apt-get install -y software-properties-common && \
  echo oracle-java16-installer shared/accepted-oracle-license-v1-2 select true | debconf-set-selections && \
  add-apt-repository -y ppa:linuxuprising/java && \
  apt-get update && \
  apt-get install -y oracle-java16-installer && \
  rm -rf /var/lib/apt/lists/* && \
  rm -rf /var/cache/oracle-jdk8-installer

# Install rust.
RUN curl -sSf https://static.rust-lang.org/rustup.sh -o rustup.sh && sh rustup.sh --yes --revision=1.50.0 && rm rustup.sh

# Create user with home directory
RUN useradd -ms /bin/bash contest
USER contest
