FROM ubuntu:24.04

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    ninja-build \
    pkg-config \
    python3 \
    python3-pip \
    python3-venv \
    iptables \
    libiptc-dev \
    libxtables-dev \
    libsystemd-dev \
    iproute2 \
    clang-format \
    clang-tidy \
    valgrind \
    lcov \
    util-linux \
 && rm -rf /var/lib/apt/lists/*

# CMake 3.28+ and Conan 2 from pip; the apt versions lag behind.
RUN pip3 install --break-system-packages --quiet --upgrade cmake conan

# Pre-builds the Conan dependencies while the image has network access. The cache stays in
# /root/.conan2 inside the image; the compose file does not mount over it.
COPY conanfile.py /tmp/conan-seed/conanfile.py
COPY profiles/ /tmp/conan-seed/profiles/
RUN conan profile detect --force \
 && conan install /tmp/conan-seed \
      --build=missing \
      --profile default \
      --profile /tmp/conan-seed/profiles/gcc13.conan \
      --output-folder /tmp/conan-seed/out \
 && rm -rf /tmp/conan-seed

# libiptc writes the legacy tables; make the iptables CLI read the same ones.
RUN update-alternatives --set iptables /usr/sbin/iptables-legacy \
 && update-alternatives --set ip6tables /usr/sbin/ip6tables-legacy

WORKDIR /workspace

COPY sandbox/entrypoint.sh /usr/local/bin/entrypoint.sh
RUN chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
CMD ["bash"]
