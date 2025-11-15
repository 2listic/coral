FROM dealii/dealii:v9.5.1-jammy

RUN sudo apt update && sudo apt install -y \
    curl \
    vim \
    build-essential \
    gnupg \
    python3-pip

RUN pip3 install --upgrade pybind11 && \
    PYBIND11_CMAKE_DIR=$(python3 -m pybind11 --cmakedir) && \
    echo "export CMAKE_PREFIX_PATH=${PYBIND11_CMAKE_DIR}:${CMAKE_PREFIX_PATH}" >> ~/.bashrc

COPY entrypoint.sh /usr/local/bin/entrypoint.sh
RUN sudo chmod +x /usr/local/bin/entrypoint.sh

ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
