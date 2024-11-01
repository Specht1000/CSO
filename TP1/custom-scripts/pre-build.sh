#!/bin/sh

BASE_DIR=$(pwd)

cp $BASE_DIR/../apps/hello $BASE_DIR/target/usr/bin/
chmod +x $BASE_DIR/target/usr/bin/hello

cp $BASE_DIR/apps/hello $BASE_DIR/output/target/etc/init.
chmod +x $BASE_DIR/output/target/etc/init.d/hello

cp $BASE_DIR/../apps/simple_http_server $BASE_DIR/target/usr/bin/
chmod +x $BASE_DIR/target/usr/bin/simple_http_server

cp $BASE_DIR/../apps/simple_http_server $BASE_DIR/output/target/etc/init.
chmod +x $BASE_DIR/output/target/etc/init.d/simple_http_server

cp $BASE_DIR/custom-scripts/S41network-config $BASE_DIR/output/target/etc/init.d/
chmod +x $BASE_DIR/output/target/etc/init.d/S41network-config

cp $BASE_DIR/custom-scripts/S41network-config $BASE_DIR/output/target/usr/bin/

cp $BASE_DIR/custom-scripts/S50hello $BASE_DIR/output/target/etc/init.d/
chmod +x $BASE_DIR/output/target/etc/init.d/S50hello

cp $BASE_DIR/custom-scripts/S50hello $BASE_DIR/output/target/usr/bin/

cp $BASE_DIR/custom-scripts/S51server $BASE_DIR/output/target/etc/init.d/
chmod +x $BASE_DIR/output/target/etc/init.d/S51server

cp $BASE_DIR/custom-scripts/S51server $BASE_DIR/output/target/usr/bin/
