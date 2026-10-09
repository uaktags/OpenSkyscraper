FROM yoottower-lab:latest

# Install Chromium, Node.js, ImageMagick, xwd, xdotool
RUN dnf install -y \
    chromium \
    nodejs \
    npm \
    ImageMagick \
    xwd \
    xdotool \
    && dnf clean all \
    && ln -sf /usr/bin/chromium-browser /usr/bin/chromium \
    && ln -sf /usr/bin/chromium-browser /usr/local/bin/chromium

# Install Bun
RUN curl -fsSL https://bun.sh/install | bash \
    && cp /root/.bun/bin/bun /usr/local/bin/bun \
    && cp /root/.bun/bin/bunx /usr/local/bin/bunx \
    && chmod +x /usr/local/bin/bun /usr/local/bin/bunx

# Configure Playwright
ENV PLAYWRIGHT_CHROMIUM_EXECUTABLE_PATH=/usr/bin/chromium
ENV PLAYWRIGHT_SKIP_BROWSER_DOWNLOAD=1

RUN cd /root && bun add playwright

WORKDIR /lab
