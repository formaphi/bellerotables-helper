
FROM almalinux:8

RUN dnf -y install epel-release dnf-plugins-core && \
  dnf config-manager --set-enabled powertools && \
  dnf -y install \
  gcc-c++ \
  cmake \
  make \
  iptables \
  iptables-services \
  iptables-devel \
  ipset \
  iproute \
  conntrack-tools \
  autoconf \
  automake \
  libtool \
  libmnl-devel \
  libnfnetlink-devel \
  libnftnl-devel \
  bzip2 \
  wget \
  python3-pip \
  clang-tools-extra \
  valgrind \
  && dnf clean all

RUN pip3 install conan gcovr && conan profile detect --force

RUN wget -q https://www.netfilter.org/projects/iptables/files/iptables-1.8.5.tar.bz2 -O /tmp/iptables.tar.bz2 && \
  tar xjf /tmp/iptables.tar.bz2 -C /tmp && \
  cd /tmp/iptables-1.8.5 && \
  ./autogen.sh && \
  ./configure --disable-nftables && \
  make -j$(nproc) && \
  make install && \
  ln -sf /usr/local/sbin/xtables-legacy-multi /usr/local/sbin/iptables-legacy && \
  rm -rf /tmp/iptables* && \
  echo 'export XTABLES_LIBDIR=/usr/lib64/xtables' >> /etc/profile.d/xtables.sh

WORKDIR /build

CMD ["sleep", "infinity"]
