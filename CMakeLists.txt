# Copyright (c) 2026, Centro de Inovação Edge.
# SPDX-License-Identifier: BSD-3-Clause

zephyr_library()
zephyr_library_sources(src/mfrc522.c)
zephyr_include_directories(includes)

zephyr_library_sources_ifdef(CONFIG_MFRC522_TRANSPORT_SPI src/transport_spi.c)
