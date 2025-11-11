FROM dealii/dealii:v9.5.1-jammy

RUN sudo apt update && sudo apt install -y \
    curl \
    vim \
    build-essential \
    gnupg

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN sudo chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
