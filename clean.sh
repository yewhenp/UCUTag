#!/usr/bin/env bash
fusermount -u mountpoint
rm -rf trash/*
mongo --eval "db.dropDatabase()" ucutag