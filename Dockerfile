# syntax=docker/dockerfile:1
ARG DEAL_II_IMAGE=dealii/dealii:v9.7.1-noble@sha256:28a40c95e76879c717a96ea3f5c87cc00ec1d7a4ce9d250ab936fb723eab6995
FROM ${DEAL_II_IMAGE} AS build
USER root
WORKDIR /src
COPY . .
ARG BUILD_JOBS=2
RUN set -eu; for config in Debug Release; do \
      cmake -S . -B "/build/$config" -GNinja \
        -DCMAKE_BUILD_TYPE="$config" -DCMAKE_INSTALL_PREFIX="/opt/dealiix/coral/$config" \
        -DCMAKE_INSTALL_LIBDIR=lib -DCORAL_BUILD_BACKEND_DEALII=ON \
        -DCORAL_BUILD_SHARED_CORE=ON -DCORAL_BUILD_TESTS=OFF -DCORAL_INSTALL=ON; \
      cmake --build "/build/$config" --target coral coral_backend_dealii -j "$BUILD_JOBS"; \
      cmake --install "/build/$config"; \
    done

FROM ${DEAL_II_IMAGE}
USER root
COPY --from=build /opt/dealiix/coral /opt/dealiix/coral
ARG VCS_REF=unknown
LABEL org.opencontainers.image.source="https://github.com/2listic/coral" \
      org.opencontainers.image.revision="${VCS_REF}"
ENV CMAKE_PREFIX_PATH=/opt/dealiix/coral/Release \
    PATH="/opt/dealiix/coral/Release/bin/Release:${PATH}"
RUN mkdir -p /work && chmod 1777 /work
WORKDIR /work
USER 1000:1000
ENTRYPOINT ["/opt/dealiix/coral/Release/bin/Release/coral"]
