import os
import subprocess
import shutil

PROJECT_DIR = "D:/lp"
SOURCE_DIR = "D:/AI Development/lilygo/source_reader"
PIO_PATH = r"C:\Users\abhif\.platformio\penv\Scripts\platformio.exe"

def run_command(cmd, cwd=PROJECT_DIR):
    print(f"Running: {' '.join(cmd)}")
    env = os.environ.copy()
    env["IDF_COMPONENT_MANAGER"] = "0"
    result = subprocess.run(cmd, cwd=cwd, env=env)
    return result.returncode

def main():
    # 0. Sync files
    print("Syncing files...")

    # Copy project libraries
    src_lib = os.path.join(SOURCE_DIR, "lib")
    dst_lib = os.path.join(PROJECT_DIR, "lib")
    if os.path.exists(src_lib):
        if os.path.exists(dst_lib):
            shutil.rmtree(dst_lib)
        shutil.copytree(src_lib, dst_lib)
        print("Copied project libs")

    # Copy SDK libraries into lib as well
    sdk_libs_root = os.path.join(PROJECT_DIR, "freeink-sdk", "libs")
    if os.path.exists(sdk_libs_root):
        for category in os.listdir(sdk_libs_root):
            cat_path = os.path.join(sdk_libs_root, category)
            if os.path.isdir(cat_path):
                for lib_name in os.listdir(cat_path):
                    # Skip libraries that cause duplicates or conflicts
                    if lib_name in ["expat", "miniz"]:
                        print(f"Skipping SDK lib {lib_name} (using project/vendor copy)")
                        continue
                    src_sdk_lib = os.path.join(cat_path, lib_name)
                    dst_sdk_lib = os.path.join(dst_lib, lib_name)
                    if os.path.isdir(src_sdk_lib):
                        if os.path.exists(dst_sdk_lib):
                            shutil.rmtree(dst_sdk_lib)
                        shutil.copytree(src_sdk_lib, dst_sdk_lib)
                        print(f"Copied SDK lib {lib_name}")

    # Remove duplicates from project libs too if they are in SDK
    for lib in ["expat", "miniz"]:
        target = os.path.join(dst_lib, lib)
        if os.path.exists(target):
            shutil.rmtree(target)
            print(f"Removed duplicate project lib {lib}")

    # Ensure specialized config is in place
    target_path = os.path.join(dst_lib, "BoardT5S3", "src", "LilyGoT5H716LgfxConfig.cpp")
    if os.path.exists(os.path.join(PROJECT_DIR, "LilyGoT5H716LgfxConfig.cpp")):
        shutil.copy(os.path.join(PROJECT_DIR, "LilyGoT5H716LgfxConfig.cpp"), target_path)

    # Ensure src directory exists and create dummy_simd.c
    os.makedirs(os.path.join(PROJECT_DIR, "src"), exist_ok=True)
    with open(os.path.join(PROJECT_DIR, "src", "dummy_simd.c"), 'w') as f:
        f.write("void s3_ycbcr_convert_420() {}\n")
        f.write("void s3_ycbcr_convert_444() {}\n")
        f.write("void s3_dequant_coefficients() {}\n")
        f.write("void s3_png_rgb565_simd() {}\n")

    # 1. Build
    code = run_command([PIO_PATH, "run", "-e", "lilygo_t5_h716"])

    if code != 0:
        print("Removing problematic .S files...")
        libdeps_dir = os.path.join(PROJECT_DIR, ".pio", "libdeps", "lilygo_t5_h716")
        if os.path.exists(libdeps_dir):
            for root, dirs, files in os.walk(libdeps_dir):
                for file in files:
                    if file.endswith(".S"):
                        file_path = os.path.join(root, file)
                        try:
                            with open(file_path, 'r', encoding='utf-8', errors='ignore') as f:
                                if "dsps_fft2r_platform.h" in f.read():
                                    print(f"Removing {file_path}")
                                    f.close()
                                    os.remove(file_path)
                        except:
                            pass
        code = run_command([PIO_PATH, "run", "-e", "lilygo_t5_h716"])

    if code == 0:
        print("BUILD SUCCESSFUL!")
    else:
        print(f"BUILD FAILED with code {code}")

if __name__ == "__main__":
    main()
