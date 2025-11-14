FROM dealii/dealii:v9.5.1-jammy

RUN sudo apt update && sudo apt install -y \
    curl \
    vim \
    build-essential \
    gnupg \
    python3-pip

RUN pip3 install --upgrade pybind11

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN sudo chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
