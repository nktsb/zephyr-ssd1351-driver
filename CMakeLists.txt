# SPDX-License-Identifier: Apache-2.0

if(CONFIG_SSD1351_CUSTOM)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})

zephyr_library()

zephyr_library_sources_ifdef(CONFIG_SSD1351_CUSTOM src/ssd1351.c)

endif(CONFIG_SSD1351_CUSTOM)