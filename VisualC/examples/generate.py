import os
import pathlib
import uuid

REPOSITORY_ROOT = pathlib.Path(__file__).parent.parent

def generate(x, y):
    guid = str(uuid.uuid4()).upper()
    text = f"""
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Label="Globals">
    <ProjectGuid>{{{guid}}}</ProjectGuid>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <ItemGroup>
    <None Include="$(SolutionDir)\\..\\examples\\{x}\\{y}\\README.txt" />
    <ClCompile Include="$(SolutionDir)\\..\\examples\\{x}\\{y}\\*.c" />
  </ItemGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />
</Project>
""".strip()

    file_name = REPOSITORY_ROOT / "VisualC" / "examples" / x / y / f"{y}.vcxproj"
    
    if file_name.exists():
        print("Skipping:", file_name)
        return

    print("Generating file:", file_name)
    os.makedirs(file_name.parent, exist_ok=True)
    with open(file_name, "w", encoding="utf-8") as f:
        f.write(text)


def main():
    path = REPOSITORY_ROOT / "examples"
    for x in path.iterdir():
        if x.is_dir():
            for y in x.iterdir():
                generate(x.name, y.name)


if __name__ == "__main__":
    main()
