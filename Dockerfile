FROM alpine:latest

# Create app directory
RUN mkdir -p /usr/src/app
WORKDIR /usr/src/app

RUN set -ex && \
    apk add --no-cache build-base linux-headers

COPY ./src /usr/src/app

RUN make
RUN ls -ltr