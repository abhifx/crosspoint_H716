import os
import subprocess
import shutil

PROJECT_DIR = os.path.dirname(os.path.abspath(__file__))
PIO_PATH = r"C:\Users\abhif\.platformio\penv\Scripts\platformio.exe"

def run_command(cmd, cwd=PROJECT_DIR):
    print(f"Running: {' '.join(cmd)}")
    env = os.environ.copy()
    env["IDF_COMPONENT_MANAGER"] = "0"
    result = subprocess.run(cmd, cwd=cwd, env=env)
    return result.returncode

def main():
    # 0. Build HTML headers
    print("Building HTML headers...")
    run_command(["python", os.path.join("scripts", "build_html.py")])

    # 1. Sync files (Simplified for current project dir)
    print("Ensuring environment is ready...")

    # 1. Build and Upload
    code = run_command([PIO_PATH, "run", "-e", "crosspoint_h716", "-t", "upload"])

    if code != 0:
        print("Build failed.")

    if code == 0:
        print("BUILD AND UPLOAD SUCCESSFUL!")
    else:
        print(f"FAILED with code {code}")

if __name__ == "__main__":
    main()
