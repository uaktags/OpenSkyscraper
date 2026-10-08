# OpenSky / YootTower lab: builds and runs OpenSky headless (tests, --sim) and
# the original YootTower under Wine (tools/yoottower/oracle), so heavy runs
# happen on a server instead of a laptop. See doc/yoottower/remote-lab.md.
FROM fedora:44

RUN dnf -y install --setopt=install_weak_deps=False \
        gcc-c++ cmake make git rsync which findutils procps-ng \
        xorg-x11-server-Xvfb xorg-x11-xauth xdpyinfo \
        libX11-devel libXrandr-devel libXcursor-devel libXi-devel libXext-devel \
        mesa-libGL-devel mesa-libEGL-devel mesa-dri-drivers systemd-devel \
        freetype-devel flac-devel libvorbis-devel libogg-devel openal-soft-devel \
        harfbuzz-devel tinyxml2-devel libmspack-devel \
        python3 python3-pip \
        wine \
    && dnf clean all \
    && pip3 install --no-cache-dir python-xlib

# Same versions the development machine builds from source.
ARG SFML_TAG=3.2.0
ARG TGUI_TAG=v1.12.1
RUN git clone --depth 1 --branch ${SFML_TAG} https://github.com/SFML/SFML.git /tmp/sfml \
    && cmake -S /tmp/sfml -B /tmp/sfml/build -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=ON \
    && cmake --build /tmp/sfml/build -j"$(nproc)" && cmake --install /tmp/sfml/build \
    && git clone --depth 1 --branch ${TGUI_TAG} https://github.com/texus/TGUI.git /tmp/tgui \
    && cmake -S /tmp/tgui -B /tmp/tgui/build -DCMAKE_BUILD_TYPE=Release -DTGUI_BACKEND=SFML_GRAPHICS \
         -DTGUI_BUILD_EXAMPLES=OFF -DTGUI_BUILD_TESTS=OFF \
    && cmake --build /tmp/tgui/build -j"$(nproc)" && cmake --install /tmp/tgui/build \
    && rm -rf /tmp/sfml /tmp/tgui && ldconfig

ENV LD_LIBRARY_PATH=/usr/local/lib64:/usr/local/lib \
    YOOT_ROOT=/lab/oracle \
    YOOT_SRC=/lab/game-src/YOOT \
    MFC42_DLL=/lab/mfc42.dll \
    OPENSKY_YOOT_DIR=/lab/game-src/YOOT
WORKDIR /lab
CMD ["sleep", "infinity"]
