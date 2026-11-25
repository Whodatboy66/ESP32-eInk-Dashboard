# miniz 3.0.2 (host tests only)

From <https://github.com/richgel999/miniz/releases/tag/3.0.2> (`miniz-3.0.2.zip`, SHA-256
`ada38db0b703a56d3dd6d57bf84a9c5d664921d870d8fea4db153979fb5332c5`), MIT License (`LICENSE`).
Unchanged. Used only by the PC tests (`tests/host/test_http_body.cpp`): the firmware uses the same
`tinfl` decompressor from the chip's ROM (`esp_rom/include/miniz.h`), so nothing of this ends up on
the device.
