#!/usr/bin/env bash

echo "Umounting ${1-mountpoint}"
fusermount -uz ${1-mountpoint}

if [[ -n "${UCUTAG_FILE_DIR}" ]]; then
    echo "Cleaning ${UCUTAG_FILE_DIR}"
    rm -rf ${UCUTAG_FILE_DIR}/*
    echo "Dropping ${UCUTAG_FILE_DIR//\//_}"
    mongo --eval "db.dropDatabase()" "ucutag${UCUTAG_FILE_DIR//\//_}"
elif [[ -d "/opt/ucutag/files" ]]; then
    echo "Cleaning /opt/ucutag/files"
    rm -rf /opt/ucutag/file/*
    echo "Dropping ucutag_opt_ucutag_files"
    mongo --eval "db.dropDatabase()" "ucutag_opt_ucutag_files"
else
    echo "Dont't know where are files to clear"
fi

