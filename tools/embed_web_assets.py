from pathlib import Path
from web_assets import generate

Import("env")

build_dir = Path(env.subst("$BUILD_DIR"))
generate(Path(env.subst("$PROJECT_DIR")) / "web", build_dir / "WebAssets.h")
env.Append(CPPPATH=[str(build_dir)])
