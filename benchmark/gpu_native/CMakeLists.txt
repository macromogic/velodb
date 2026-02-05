# GPU Native TPC-H Benchmark This is designed to be included from the main
# project CMakeLists.txt

# GPU Native Benchmark executable
add_executable(gpu_native_benchmark ${CMAKE_CURRENT_SOURCE_DIR}/main.cu)

target_include_directories(
  gpu_native_benchmark
  PRIVATE ${CMAKE_CURRENT_SOURCE_DIR} ${CMAKE_SOURCE_DIR}/thirdparty
          ${CMAKE_SOURCE_DIR}/thirdparty/argparse/include)

target_link_libraries(gpu_native_benchmark PRIVATE fmt::fmt argparse::argparse
                                                   CUDA::cudart)

# CUDA compile options
target_compile_options(
  gpu_native_benchmark PRIVATE $<$<COMPILE_LANGUAGE:CUDA>: --extended-lambda
                               --expt-relaxed-constexpr >)

# Enable cooperative launch for grid sync
set_target_properties(
  gpu_native_benchmark
  PROPERTIES CUDA_SEPARABLE_COMPILATION ON
             CUDA_RESOLVE_DEVICE_SYMBOLS ON
             RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# Install target
install(TARGETS gpu_native_benchmark RUNTIME DESTINATION bin
                                             COMPONENT benchmark)
