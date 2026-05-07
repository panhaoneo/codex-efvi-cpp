# USAGE

## 环境要求
- Linux, GCC 7+, CMake 3.10+
- OpenOnload 8.1.26（真实网卡模式）

## 构建（Mock）
```bash
cmake -S . -B build -DEFVI_MOCK_MODE=ON
cmake --build build
ctest --test-dir build
```
