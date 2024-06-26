FROM dealii/dealii:v9.5.1-jammy

RUN sudo apt update && sudo apt install -y \
    curl \
    gnupg \
    build-essential

RUN  curl -fsSL https://deb.nodesource.com/setup_22.x | sudo bash -

RUN  sudo apt install -y nodejs

RUN node -v && npm -v

WORKDIR /usr/src/app

COPY . .

WORKDIR /usr/src/app

RUN sudo npm install

EXPOSE 8080

CMD ["sudo","npm", "run", "serve"]
