FROM almalinux:8

RUN dnf -y install \
  gcc-c++ \
  cmake \
  make \
  iptables \
  iptables-services \
  iptables-devel \
  ipset \
  iproute \
  conntrack-tools \
  && dnf clean all

WORKDIR /build
COPY CMakeLists.txt main.cpp iptc_builder.h iptc_target.h ./

RUN cmake -B out -DCMAKE_BUILD_TYPE=Debug && cmake --build out

CMD ["sleep", "infinity"]
