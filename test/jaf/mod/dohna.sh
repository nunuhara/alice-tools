#!/bin/sh
cd $(dirname "$0")/dohna
env LANG=ja_JP.UTF-8 wine dohnadohna.exe
