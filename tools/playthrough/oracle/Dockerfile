FROM i386/debian:bookworm

ENV DEBIAN_FRONTEND=noninteractive
ENV WINEARCH=win32
ENV WINEPREFIX=/lab/wine32
ENV DISPLAY=:99

RUN apt-get update && apt-get install -y --no-install-recommends \
    wine \
    wine32 \
    xvfb \
    xauth \
    xdotool \
    imagemagick \
    x11-apps \
    python3-pil \
    python3 \
    procps \
    gosu \
    && rm -rf /var/lib/apt/lists/*

# Non-root user 'lab'
RUN useradd -m -u 1000 -s /bin/bash lab

# Set ImageMagick policy to allow standard operations
RUN sed -i 's/rights="none" pattern="PDF"/rights="read|write" pattern="PDF"/' /etc/ImageMagick-6/policy.xml || true

WORKDIR /lab

COPY entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["sleep", "infinity"]
