#!/bin/bash
set -e

STARTUP_SCRIPT="/app/docker/startup.sh"
if [ -f ${STARTUP_SCRIPT} ]; then
    source ${STARTUP_SCRIPT}
fi

tail -f /dev/null
