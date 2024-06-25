FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    curl \
    gnupg \
    build-essential

RUN curl -fsSL https://deb.nodesource.com/setup_22.x | bash -

RUN apt-get install -y nodejs

RUN node -v && npm -v

WORKDIR /usr/src/app

COPY . .

RUN npm install

RUN apt update && apt install libdeal.ii-dev libdeal.ii-doc -y

WORKDIR /usr/src/app

COPY package*.json ./

RUN npm install

COPY . .

EXPOSE 8080

CMD ["npm", "run", "serve"]
