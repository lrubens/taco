# Use a smaller base image
FROM ubuntu:20.04

ENV SUITESPARSE_PATH=/app/data/suitesparse \
    FROST_PATH=/app/data/FROSTT \
    DEBIAN_FRONTEND=noninteractive

# Set the working directory
WORKDIR /app

# Install dependencies
RUN apt-get update && apt-get install -y \
  apt-utils \
  build-essential \
  python3 \
  python3-pip \
    cmake \
  git \
  curl \
  unzip \
  libomp-dev \
  zlib1g-dev \
  libssl-dev \
  libprotobuf-dev \
  protobuf-compiler \
  libopenmpi-dev && \
  apt-get clean && \
  rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

COPY baco /app/baco

  #git clone https://github.com/baco-authors/baco.git && \
RUN \
  cd baco && \
  apt-get install -y pip && \
  pip install --upgrade pip && \
  pip install -e .
RUN export PYTHONPATH="/app/baco"
ENV PYTHONPATH="/app/baco"
RUN \
  cd /app/baco && \
  bash install_baselines.sh && cd /app

# # Clone and build protobuf from source
# RUN git clone https://github.com/protocolbuffers/protobuf.git && \
#     cd protobuf && \
#     git checkout v3.17.3 && \
#     mkdir build && cd build && \
#     cmake ../cmake/ -Dprotobuf_BUILD_TESTS=OFF && \
#     make -j8 && \
#     make install && \
#     cd ../.. && \
#     rm -rf protobuf

# RUN git clone --recurse-submodules -b v1.56.0 --depth 1 --shallow-submodules https://github.com/grpc/grpc

# Copy current directory files to docker
# COPY grpc_install.sh .
# RUN ./grpc_install.sh

COPY . .
# Create build directory, build the project, and clean up
RUN mkdir build && \
    cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DOPENMP=ON .. && \
    make -j8 && \
    mv ../cpp_taco_* . && \
    cd ..

# Here we assume that "cpp_taco_*" files are meant to stay in "/app/build". 
# If that's not the case, please adjust the path accordingly.

COPY run_taco.sh .
ENTRYPOINT ["./run_taco.sh"]
CMD ["-mat", "Goodwin_040/Goodwin_040.mtx", "--method", "random", "-o", "SpMM"]


