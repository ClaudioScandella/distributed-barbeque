#!/bin/bash

LANG=${1:-'cpp'}

cd $LANG
tar cjf ../payload.tar.bz2 ./*
cd ..

if ! [ -e 'payload.tar.bz2' ]; then
	echo "ERROR: failed packaging [$LANG] application template"
	exit 1
fi

cat decompressor.sh payload.tar.bz2 > bbque_template_$LANG.bsx
chmod +x bbque_template_$LANG.bsx

echo "bbque_template_$LANG.bsx created"
exit 0
