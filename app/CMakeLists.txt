# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(PanduzaHA-ng)

set(app_sources
    src/main.c
    src/app_mqtt.c
    src/panduza.c
    src/panduza_dio.c
)
target_sources(app PRIVATE ${app_sources})
