FROM nvidia/vulkan:1.2.133-450
ENV DEBIAN_FRONTEND noninteractive

RUN apt-get update && apt-get -y upgrade && \
    apt-get install -y --no-install-recommends \
    libx11-xcb-dev \
    libxkbcommon-dev \
    libwayland-dev \
    libxrandr-dev \
    libegl1-mesa-dev \
    libxinerama-dev \
    libxcursor-dev \
    libxi-dev \
    libxxf86vm-dev \
    build-essential \
    git \
    vim \
    cmake \
    screenfetch \
    htop \
    wget \
    git && \
    rm -rf /var/lib/apt/lists/* /usr/local/include/*

RUN wget -O vulkansdk.tar.gz https://sdk.lunarg.com/sdk/download/1.3.204.1/linux/vulkansdk-linux-x86_64-1.3.204.1.tar.gz
RUN tar xvf vulkansdk.tar.gz
RUN mkdir /usr/local/VulkanSDK && mv 1.3.204.1 /usr/local/VulkanSDK
RUN rm vulkansdk.tar.gz
RUN rm -rf /usr/local/bin/*
RUN rm -rf /usr/local/share/vulkan
RUN rm -rf /usr/local/lib/*

ENV VULKAN_SDK="/usr/local/VulkanSDK/1.3.204.1/x86_64"
ENV PATH="$VULKAN_SDK/bin:$PATH"
ENV LD_LIBRARY_PATH="$VULKAN_SDK/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
ENV VK_LAYER_PATH="$VULKAN_SDK/etc/vulkan/explicit_layer.d"