# miniz 3.0.2

From <https://github.com/richgel999/miniz/releases/tag/3.0.2> (`miniz-3.0.2.zip`, SHA-256
`ada38db0b703a56d3dd6d57bf84a9c5d664921d870d8fea4db153979fb5332c5`), MIT License (`LICENSE`).
Unchanged.

- Firmware: only the decompressor (tinfl), compiled by `src/services/Inflate.c`, used by
  `src/services/HttpBody.h` for packed (gzip) calendar downloads. The chip's ROM has an older tinfl
  (miniz 1.15); with it the packed downloads failed on the device (builds 83-84).
- PC tests: the whole library (`tests/host/test_http_body.cpp` also packs its test data with it).
