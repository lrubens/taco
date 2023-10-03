# Use a smaller base image
FROM ubuntu:20.04
# FROM pytorch/pytorch

ENV SUITESPARSE_PATH=/home/data/suitesparse \
  FROST_PATH=/home/data/FROSTT \
  DEBIAN_FRONTEND=noninteractive

# Set the working directory
WORKDIR /home

# Install dependencies
RUN apt-get update && apt-get install -y \
  apt-utils \
  build-essential \
  python3.10 \
  python3-pip \
  cmake \
  git \
  curl \
  unzip \
  libomp-dev \
  zlib1g-dev \
  libssl-dev \
  ruby \
  autoconf automake libtool libgsl-dev \
  pkg-config \
  libopenmpi-dev && \
  apt-get clean && \
  rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

RUN pip3 install torch torchvision torchaudio --index-url https://download.pytorch.org/whl/cpu

# install baco
RUN \
  cd /home && \
  git clone https://github.com/baco-authors/baco.git && \
  cd baco && \
  apt-get install -y pip && \
  pip3 install --upgrade pip && \
  pip3 install -e .

# install baselines
RUN \
  cd /home/baco && \
  bash install_baselines.sh && cd /home

# set environment variables
ENV PYTHONPATH="/home/baco:/home/baco/extra_packages/CCS/bindings/python/"
ENV LD_LIBRARY_PATH="/usr/local/lib"

RUN git clone https://github.com/lrubens/taco.git && cd taco && git checkout grpc && cd . && cd /home/taco

# Create build directory, build the project, and clean up
RUN cd /home/taco && mkdir build && \
  cd build && \
  cmake -DCMAKE_BUILD_TYPE=Release -DOPENMP=ON .. && \
  make -j16 && \
  mv ../cpp_taco_* . && \
  cd ..

# Here we assume that "cpp_taco_*" files are meant to stay in "/app/build". 
# If that's not the case, please adjust the path accordingly.

RUN apt-get update && apt-get install numactl -y
ENV HYPERMAPPER_HOME=/home/baco
RUN cd - && cd - && cd - && cd -
WORKDIR /home/taco
COPY taco_run.sh build/
WORKDIR /home/taco/build
# ENTRYPOINT ["/home/taco/build/taco_run.sh"]
# CMD ["-mat", "Goodwin_040/Goodwin_040.mtx", "--method", "random", "-o", "SpMM"]


